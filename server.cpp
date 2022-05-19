#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <time.h>
#include <wiringPi.h>

int OFFSET_t = 300000;
int PIN_LED = 7;
int PIN_SHUTTER = 16;
int PIN_FOCUS = 15;

std::string PI_ROOT = "/home/pi/Documents/Intervalometre";
std::string ROOT_WEB = PI_ROOT + "/web-pages2";
std::string ROOT_EXEC = "/home/pi/Documents/Intervalometre";

pid_t PID = -1;
int child = -1;

int create_tcp_server(int port) {
  int server_sock;

  server_sock = socket(AF_INET, SOCK_STREAM, 0); // IPv4 + TCP
  if (server_sock == -1) {
    perror("socket");
    return -1;
  }

  int yes = 1;
  if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
    perror("setsockopt");
    close(server_sock);
    return -1;
  }

  sockaddr_in server_addr{};
  // Any local IPv4 address
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof server_addr) == -1) {
    perror("bind");
    close(server_sock);
    return -1;
  }

  if (listen(server_sock, 10) == -1) {
    perror("listen");
    close(server_sock);
    return -1;
  }

  return server_sock;
}

std::string make_header(int code, std::string file_type, int msg_size){
  
  // creating time of request and response
  time_t rawtime;
  struct tm rqst_time, expr_time;
  time(&rawtime);
  rqst_time = *gmtime(&rawtime);
  expr_time = *gmtime(&rawtime);
  mktime(&expr_time);
  // convert char* to string and deleting the last character as it's an '\n'
  std::string rqst_time_str = std::string(asctime(&rqst_time));
  rqst_time_str = rqst_time_str.substr(0,rqst_time_str.size()-1);
  
  std::string expr_time_str = std::string(asctime(&expr_time));
  expr_time_str = expr_time_str.substr(0,expr_time_str.size()-1);
  
  std::string header;
  if (code == 200){
    header += "HTTP/1.1 200 OK\r\n";
    header += "Date: "+rqst_time_str+" GMT\r\n";
    header += "Expires: -1\r\n";
    header += "Cache-Control: private, max-age=0\r\n";
    header += "Content-Type: "+file_type+";";
    header += "charset=UTF-8\r\n";
    header += "Content-Length: " + std::to_string(msg_size) + "\r\n\r\n";
    return header;
  }
  else if (code == 304){
    header += "HTTP/1.1 304 Not Modifed\r\n";
    header += "Date: "+rqst_time_str+" GMT\r\n";
    header += "Expires: "+expr_time_str+" GMT\r\n";
    header += "Cache-Control: max-age=3600, immutable\r\n\r\n";
    return header;
  }
  else if (code == 400){
    header = "HTTP/1.1 400 Bad request\r\n\r\n";
    return header;
  }
  else if (code == 404){
    header += "HTTP/1.1 404 Not found\r\n\r\n";
    return header;
  }
  return header;
}

void get_handler(std::string request, std::string &request_file_name, std::string &request_file_type){
  // extract de file requested by the client
  std::string get_subrequest = request.substr(request.find(" ")+1);
  request_file_name = get_subrequest.substr(0, get_subrequest.find(" "));
  if (request_file_name == "/"){
    request_file_name = "/index.html";
    
    // extract the type of file that the client accept
    std::string accept_subrequest = request.substr(request.find("Accept:"));
    int type_found = accept_subrequest.find(" ")+1;
    request_file_type = accept_subrequest.substr(type_found, (accept_subrequest.find(",")-type_found));
    std::cout << "Type of file accepted: \033[32m" << request_file_type << "\033[m\n";
  }
  else if (request_file_name == "/favicon.ico"){
    request_file_name = "/images/favicon.ico";
    request_file_type = std::string("image");
    std::cout << "Type of file accepted: \033[32m" << request_file_type << "\033[m\n";
  }
  std::cout << "Requested file: \033[32m" << request_file_name << "\033[m\n";

}

void send_response(const int &client_sock, const std::string &request_file_name, const std::string &request_file_type){
  int code = 0;
  std::string message;
  std::ifstream file(ROOT_WEB+request_file_name);
  if (file){
    code = 200;
    message = std::string((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));
  }
  else{
    code = 404;
    std::cout << "File not found: Error 404\n";
    message = "<html><head><title>Star tracker website</title>";
    message += "<meta charset='utf-8'/><link type='text/css' rel='stylesheet'";
    message += "href='style.css' /><head/><body>Error 404: File Not Found";
    message += "</body></html>";
  }

  std::string header = make_header(code, request_file_type, message.size());
  
  std::string response = header + message;
  if (send(client_sock, response.c_str(), response.size(), 0) == -1) {
    perror("send");
  }
}

void interval(const float &t_pose, const int &nb_photo, const float &wait){
    std::cout << "temps de pose = " << t_pose << " s, nombre de photo = " << nb_photo << ", enregistrement = " << wait << "s\n";

    digitalWrite(PIN_SHUTTER, true);
    usleep(OFFSET_t/2);
    digitalWrite(PIN_SHUTTER, false);
    usleep(OFFSET_t);
    long t = long(t_pose * 1000000.0);
    for (int i=0; i<nb_photo; i++){
      std::cout << "Photo n°" << i+1 << " : temps de pose = " << t_pose << "\n";
      digitalWrite(PIN_SHUTTER, true);
      usleep(OFFSET_t);
      usleep(t);
      digitalWrite(PIN_SHUTTER, false);
      std::cout << "Low\n";
      usleep(wait * 1000000.0);
    }
}

void post_handler(std::string request, const int &client_sock){
  // extract de file requested by the client
  std::string post_subrequest = request.substr(request.find_last_of("\n")+1);
  std::cout << "Post data : \033[32m" << post_subrequest << "\033[m\n";
  
  if (post_subrequest.find("stop") != std::string::npos){
    if (child != -1){
      std::cout << "STOP ! STOP ! Stop le programme " << child << "\n";
      std::string exec_args = "kill -2 "+std::to_string(child);
      system(exec_args.c_str());
      digitalWrite(PIN_SHUTTER, false);
    }
  }
  else if (post_subrequest.find("shutdown") != std::string::npos){
    int myPID = getpid();
    std::cout << "FIN ! FIN ! Fin du programme " << myPID << "\n";
    std::string exec_args = "kill -2 "+std::to_string(myPID)+" & sudo shutdown 0";
    digitalWrite(PIN_SHUTTER, false);
    digitalWrite(PIN_LED, false);
    std::cout << "!!!!!!!!!!!!!!!!!!!!! Attention le programme à été arrêté et la raspberry va s'éteindre !!!!!!!!!!!!!!!!!!!!\n";
    if (child != -1){
      std::string exec_arg = "kill -2 "+std::to_string(child);
      system(exec_arg.c_str());
    }
    send_response(client_sock, "/out.html", "text/html");
    system(exec_args.c_str());
    
    exit(0);
  }
  else{
    std::string arg = post_subrequest.substr(post_subrequest.find("=")+1);
    std::string arg_1 = arg.substr(0,arg.find("&"));
    
    arg = arg.substr(arg.find("=")+1);
    std::string arg_2 = arg.substr(0,arg.find("&"));
    
    std::string arg_3 = arg.substr(arg.find("=")+1);
    
    PID = fork();
    
    if (PID == 0){
      std::cout << "Processus fils : pid = " << getpid() << "\n";
      
      // close the connection with this client
      close(client_sock);
      
      int nb_photo = std::stoi(arg_1);
      float t_pose = std::stof(arg_2);
      float wait = std::stof(arg_3);
      
      interval(t_pose, nb_photo, wait);
      
      child = -1;
      std::cout << "Fin du fork, le PID du fils retourne à -1\n";

      exit(0);
    }
    else{
      child = PID;
      std::cout << "Parent : child = " << child << "\n";      
    }
  }
}


int main() {
  
  const int PORT = 55000;

  int server_sock = create_tcp_server(PORT);
  if (server_sock == -1) {
    std::cerr << "Failed to create tcp server\n";
    return 1;
  }
  
  std::cout << "listening on port " << PORT << " ...\n";
  
  wiringPiSetup();
  if (wiringPiSetup()<0){
    std::cout << "Setup wiringPi failed\n";
    exit(0);
  }
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_SHUTTER, OUTPUT);
  pinMode(PIN_FOCUS, OUTPUT);
  digitalWrite(PIN_FOCUS, false);
  usleep(2500);
  digitalWrite(PIN_LED, true);
  usleep(2500000);
  digitalWrite(PIN_LED, false);
  usleep(2500000);
  digitalWrite(PIN_LED, true);
  digitalWrite(PIN_SHUTTER, true);
  usleep(OFFSET_t-5000);
  digitalWrite(PIN_SHUTTER, false);
  usleep(OFFSET_t);

  while (true) {
    sockaddr_in client_addr;
    socklen_t sin_size = sizeof client_addr;

    // the accept syscall blocks until a new client connects
    // and then returns a new socket that can be used to exchange data with the client
    int client_sock = accept(server_sock, reinterpret_cast<sockaddr*>(&client_addr), &sin_size);
    if (client_sock == -1) {
      perror("accept");
      continue;
    }

    char client_name[128];
    if (!inet_ntop(client_addr.sin_family, reinterpret_cast<sockaddr*>(&client_addr.sin_addr),
		   client_name, sizeof client_name)) {
      perror("inet_ntop");
      strcpy(client_name, "<UNKNOWN>");
    }

    std::cout << "\n\033[34m=======================================\033[m\n";
    std::cout << "connection from client \033[31m" << client_name << ':' << ntohs(client_addr.sin_port) << "\033[m\n";

    char buffer[1024];
    // read some bytes over the network connection
    ssize_t n = recv(client_sock, buffer, (sizeof buffer) - 1, 0);
    if (n == -1) {
      perror("recv");
      continue;
    }
    // add a final '\0' to make it printable
    buffer[n] = '\0';
    std::string request = std::string(buffer);
    std::cout << "received \033[31m" << n << " bytes\033[m\n"	<< '"' << buffer << "\"\n";
    
    //int found = request.find("GET");
    std::string request_file_name;
    std::string request_file_type;
    
    if (int(request.find("GET")) != -1){
      get_handler(request, request_file_name, request_file_type);
      
      send_response(client_sock, request_file_name, request_file_type);
    }
    
    else if (int(request.find("POST")) != -1){
      post_handler(request, client_sock);
      
      get_handler(request, request_file_name, request_file_type);
      
      send_response(client_sock, request_file_name, request_file_type);
    }

    // close the connection with this client
    close(client_sock);
  }

  // close the server socket
  close(server_sock);

  return 0;
}

