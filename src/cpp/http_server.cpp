#include "warehouse_service.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }

std::string json_escape(const std::string& text) { std::string out; for (unsigned char c : text) { switch(c) { case '"':out+="\\\"";break;case '\\':out+="\\\\";break;case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break;default: if(c<32) out+=" "; else out+=static_cast<char>(c); } } return out; }
std::string q(const char* v) { return "\"" + json_escape(v ? v : "") + "\""; }
std::string product_json(const DSProduct& p) { std::ostringstream o;o<<"{\"id\":"<<q(p.id)<<",\"sku\":"<<q(p.sku)<<",\"name\":"<<q(p.name)<<",\"supplier_id\":"<<q(p.supplier_id)<<",\"category\":"<<q(p.category)<<",\"quantity\":"<<p.quantity<<",\"reorder_level\":"<<p.reorder_level<<",\"price\":"<<p.price<<"}";return o.str(); }
std::string supplier_json(const DSSupplier& s) { return "{\"id\":"+q(s.id)+",\"name\":"+q(s.name)+",\"contact\":"+q(s.contact)+",\"email\":"+q(s.email)+"}"; }
std::string order_json(const DSOrder& o) { return "{\"id\":"+q(o.id)+",\"product_id\":"+q(o.product_id)+",\"customer\":"+q(o.customer)+",\"quantity\":"+std::to_string(o.quantity)+",\"priority\":"+std::to_string(o.priority)+"}"; }
std::string action_json(const DSAction& a) { return "{\"description\":"+q(a.description)+",\"product_id\":"+q(a.product_id)+",\"stock_delta\":"+std::to_string(a.stock_delta)+",\"undoable\":"+(a.undoable?"true":"false")+"}"; }
template <typename T> std::string array_json(const std::vector<T>& values, const std::function<std::string(const T&)>& render) { std::string out="["; for(size_t i=0;i<values.size();++i){if(i)out+=',';out+=render(values[i]);}return out+"]"; }

std::string url_decode(const std::string& s) { std::string out; for(size_t i=0;i<s.size();++i){if(s[i]=='+')out+=' ';else if(s[i]=='%'&&i+2<s.size()){unsigned value=0;std::istringstream(s.substr(i+1,2))>>std::hex>>value;out+=static_cast<char>(value);i+=2;}else out+=s[i];}return out; }
std::map<std::string,std::string> form(const std::string& body){std::map<std::string,std::string> v;size_t start=0;while(start<=body.size()){size_t end=body.find('&',start);auto pair=body.substr(start,end==std::string::npos?std::string::npos:end-start);auto eq=pair.find('=');v[url_decode(pair.substr(0,eq))]=url_decode(eq==std::string::npos?"":pair.substr(eq+1));if(end==std::string::npos)break;start=end+1;}return v;}
bool integer(const std::string& s,int& out){if(s.empty())return false;size_t pos=0;try{long v=std::stol(s,&pos);if(pos!=s.size()||v<-100000000||v>100000000)return false;out=static_cast<int>(v);return true;}catch(...){return false;}}
bool decimal(const std::string& s,double& out){if(s.empty())return false;size_t pos=0;try{out=std::stod(s,&pos);return pos==s.size()&&std::isfinite(out);}catch(...){return false;}}
bool copy_field(const std::map<std::string,std::string>& f,const char* key,char* out,size_t cap,bool required=true){auto it=f.find(key);if(it==f.end()){if(required)return false;out[0]=0;return true;}if((required&&it->second.empty())||it->second.size()>=cap)return false;std::snprintf(out,cap,"%s",it->second.c_str());return true;}
std::string content_type(const std::string& path){if(path.ends_with(".css"))return"text/css; charset=utf-8";if(path.ends_with(".js"))return"application/javascript; charset=utf-8";return"text/html; charset=utf-8";}

class Server {
public:
    Server(warex::WarehouseService& service, std::filesystem::path frontend) : service_(service), frontend_(std::move(frontend)) {}
    int serve(int port) {
        int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0){std::cerr<<"Could not create server socket.\n";return 1;}int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(static_cast<uint16_t>(port));
        if(bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof address)<0||listen(fd,16)<0){std::cerr<<"Could not start on port "<<port<<": "<<std::strerror(errno)<<"\n";close(fd);return 1;}
        std::cout<<"WAREX running at http://127.0.0.1:"<<port<<"\nPress Ctrl+C to stop.\n";
        while(running){int client=accept(fd,nullptr,nullptr);if(client<0){if(errno==EINTR)continue;break;}handle(client);close(client);}close(fd);return 0;
    }
private:
    warex::WarehouseService& service_; std::filesystem::path frontend_;
    void reply(int fd,int status,const std::string& body,const std::string& type="application/json; charset=utf-8") { const char* text=status==200?"OK":status==201?"Created":status==204?"No Content":status==400?"Bad Request":status==404?"Not Found":"Internal Server Error";std::ostringstream out;out<<"HTTP/1.1 "<<status<<' '<<text<<"\r\nContent-Type: "<<type<<"\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\n\r\n"<<body;auto data=out.str();send(fd,data.data(),data.size(),0); }
    void result(int fd,const warex::Result& r,int success=200){reply(fd,r.ok?success:400,"{\"ok\":"+std::string(r.ok?"true":"false")+",\"message\":\""+json_escape(r.message)+"\"}");}
    void handle(int fd) {
        std::string request;char buffer[4096];while(request.find("\r\n\r\n")==std::string::npos&&request.size()<65536){auto got=recv(fd,buffer,sizeof buffer,0);if(got<=0)return;request.append(buffer,static_cast<size_t>(got));}
        auto split=request.find("\r\n\r\n");if(split==std::string::npos){reply(fd,400,"{\"ok\":false,\"message\":\"Malformed request.\"}");return;}std::istringstream head(request.substr(0,split));std::string method,target,version;head>>method>>target>>version;std::string line;size_t length=0;while(std::getline(head,line)){if(line.rfind("Content-Length:",0)==0)try{length=std::stoul(line.substr(15));}catch(...){}}
        std::string body=request.substr(split+4);while(body.size()<length){auto got=recv(fd,buffer,sizeof buffer,0);if(got<=0)break;body.append(buffer,static_cast<size_t>(got));}if(body.size()>length)body.resize(length);route(fd,method,target,body);
    }
    void route(int fd,const std::string& method,const std::string& target,const std::string& body) {
        auto question=target.find('?');std::string path=target.substr(0,question);std::string query=question==std::string::npos?"":target.substr(question+1);auto fields=form(body);auto query_values=form(query);
        if(method=="GET"&&path=="/api/summary"){auto s=service_.snapshot();std::ostringstream o;o<<"{\"products\":"<<s.products<<",\"suppliers\":"<<s.suppliers<<",\"normal_orders\":"<<s.normal_orders<<",\"priority_orders\":"<<s.priority_orders<<",\"low_stock\":"<<s.low_stock<<",\"inventory_value\":"<<s.inventory_value<<"}";reply(fd,200,o.str());return;}
        if(method=="GET"&&path=="/api/products"){bool alpha=query_values["sort"]!="quantity";auto data=alpha?service_.products(true):service_.stock_report(query_values["direction"]!="desc");reply(fd,200,array_json<DSProduct>(data,product_json));return;}
        if(method=="GET"&&path=="/api/products/search"){auto found=service_.find_product_by_name(query_values["name"]);reply(fd,200,found?product_json(*found):"null");return;}
        if(method=="GET"&&path=="/api/suppliers"){auto data=service_.suppliers();reply(fd,200,array_json<DSSupplier>(data,supplier_json));return;}
        if(method=="GET"&&path=="/api/orders"){auto normal=service_.normal_orders(), priority=service_.priority_orders();reply(fd,200,"{\"normal\":"+array_json<DSOrder>(normal,order_json)+",\"priority\":"+array_json<DSOrder>(priority,order_json)+"}");return;}
        if(method=="GET"&&path=="/api/low-stock"){auto data=service_.low_stock();reply(fd,200,array_json<DSProduct>(data,product_json));return;}
        if(method=="GET"&&path=="/api/actions"){auto data=service_.actions();reply(fd,200,array_json<DSAction>(data,action_json));return;}
        if(method=="POST"&&path=="/api/demo"){result(fd,service_.seed_demo_data());return;}
        if(method=="POST"&&path=="/api/products"){DSProduct p{};int qv,reorder;double price;if(!copy_field(fields,"id",p.id,sizeof p.id)||!copy_field(fields,"sku",p.sku,sizeof p.sku)||!copy_field(fields,"name",p.name,sizeof p.name)||!copy_field(fields,"supplier_id",p.supplier_id,sizeof p.supplier_id,false)||!copy_field(fields,"category",p.category,sizeof p.category)||!integer(fields["quantity"],qv)||!integer(fields["reorder_level"],reorder)||!decimal(fields["price"],price)){reply(fd,400,"{\"ok\":false,\"message\":\"Enter complete product details using valid numbers.\"}");return;}p.quantity=qv;p.reorder_level=reorder;p.price=price;result(fd,service_.add_product(p),201);return;}
        if(method=="PUT"&&path.rfind("/api/products/",0)==0){DSProduct p{};std::string id=url_decode(path.substr(14));int qv,reorder;double price;if(id.size()>=sizeof p.id||!copy_field(fields,"sku",p.sku,sizeof p.sku)||!copy_field(fields,"name",p.name,sizeof p.name)||!copy_field(fields,"supplier_id",p.supplier_id,sizeof p.supplier_id,false)||!copy_field(fields,"category",p.category,sizeof p.category)||!integer(fields["quantity"],qv)||!integer(fields["reorder_level"],reorder)||!decimal(fields["price"],price)){reply(fd,400,"{\"ok\":false,\"message\":\"Enter valid product details.\"}");return;}std::snprintf(p.id,sizeof p.id,"%s",id.c_str());p.quantity=qv;p.reorder_level=reorder;p.price=price;result(fd,service_.update_product(p));return;}
        if(method=="DELETE"&&path.rfind("/api/products/",0)==0){result(fd,service_.delete_product(url_decode(path.substr(14))));return;}
        if(method=="POST"&&path.rfind("/api/products/",0)==0&&path.ends_with("/stock")){int delta;auto id=url_decode(path.substr(14,path.size()-20));if(!integer(fields["delta"],delta)){reply(fd,400,"{\"ok\":false,\"message\":\"Stock delta must be a whole number.\"}");return;}result(fd,service_.adjust_stock(id,delta));return;}
        if(method=="POST"&&path=="/api/suppliers"){DSSupplier s{};if(!copy_field(fields,"id",s.id,sizeof s.id)||!copy_field(fields,"name",s.name,sizeof s.name)||!copy_field(fields,"contact",s.contact,sizeof s.contact)||!copy_field(fields,"email",s.email,sizeof s.email)){reply(fd,400,"{\"ok\":false,\"message\":\"Enter complete supplier details.\"}");return;}result(fd,service_.add_supplier(s),201);return;}
        if(method=="PUT"&&path.rfind("/api/suppliers/",0)==0){DSSupplier s{};std::string id=url_decode(path.substr(15));if(id.size()>=sizeof s.id||!copy_field(fields,"name",s.name,sizeof s.name)||!copy_field(fields,"contact",s.contact,sizeof s.contact)||!copy_field(fields,"email",s.email,sizeof s.email)){reply(fd,400,"{\"ok\":false,\"message\":\"Enter valid supplier details.\"}");return;}std::snprintf(s.id,sizeof s.id,"%s",id.c_str());result(fd,service_.update_supplier(s));return;}
        if(method=="DELETE"&&path.rfind("/api/suppliers/",0)==0){result(fd,service_.delete_supplier(url_decode(path.substr(15))));return;}
        if(method=="POST"&&path=="/api/orders"){DSOrder o{};int qty,priority;if(!copy_field(fields,"id",o.id,sizeof o.id)||!copy_field(fields,"product_id",o.product_id,sizeof o.product_id)||!copy_field(fields,"customer",o.customer,sizeof o.customer)||!integer(fields["quantity"],qty)||!integer(fields["priority"],priority)){reply(fd,400,"{\"ok\":false,\"message\":\"Enter complete order details.\"}");return;}o.quantity=qty;o.priority=priority;result(fd,service_.add_order(o),201);return;}
        if(method=="POST"&&path=="/api/orders/process"){result(fd,service_.process_next_order());return;}
        if(method=="POST"&&path=="/api/actions/undo"){result(fd,service_.undo_last_action());return;}
        if(method=="GET"&&path=="/"){std::ifstream in(frontend_);if(!in){reply(fd,500,"WAREX UI file is unavailable.","text/plain");return;}std::ostringstream page;page<<in.rdbuf();reply(fd,200,page.str(),content_type(frontend_.string()));return;}
        reply(fd,404,"{\"ok\":false,\"message\":\"Route not found.\"}");
    }
};
}

int main(int argc,char**argv){int port=8080;if(argc>1)try{port=std::stoi(argv[1]);}catch(...){std::cerr<<"Usage: warex [port]\n";return 2;}if(port<1024||port>65535){std::cerr<<"Port must be between 1024 and 65535.\n";return 2;}std::signal(SIGINT,stop);std::signal(SIGTERM,stop);std::filesystem::path source=WAREX_SOURCE_DIR;warex::WarehouseService service((std::filesystem::current_path()/"data"/"warehouse.wrx").string());auto loaded=service.load();std::cout<<loaded.message<<'\n';return Server(service,source/"frontend"/"index.html").serve(port);}
