/* C++ standard include files first */
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <sstream>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <map>
#include <thread>
#include <mutex>


using namespace std;

/* C system include files next */
#include <arpa/inet.h>
#include <netdb.h>

/* C standard include files next */
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <sys/time.h>

/* your own include last */
#include "my_socket.h"
#include "my_readwrite.h"
#include <sys/stat.h>
#include "my_timestamp.h"


/* global variables */
static int master_socket_fd; /* there is nothing wrong with using a global variable */

string host = "127.0.0.1";
string port = "12345";
string URI;
string rootdir = "lab4data";
string output_file;
string pid_file;

// for loggin
ostream *mylog=NULL;
static ofstream myfile;
string logFile;

map<string, double> throttleMap;
map<string, vector<int> > throttle_vector_map;

class Connection {
public: /* I know this is bad form, but this make things easier (and shorter) */
    int conn_number; /* -1 means that the connection is not initialized properly */
    int socket_fd; /* -1 means closed by connection-handling thread, -2 means close by console thread and connection may still be active */
    int orig_socket_fd; /* copy of the original socket_fd */
    string ip_and_port_info;
    double p;
    string uri;
    double maxr;
    string starttime;
    double bytecount;
    int filelength;
    int secondElapsed;
    double dial;
    double downloadSpeedFactor;
    shared_ptr<thread> thread_ptr; /* shared pointer to a thread object */
    int kb_sent; /* number of KB (including partial KB at the end) of response body written into the socket */
    Connection() : conn_number(-1), socket_fd(-1), ip_and_port_info(""), thread_ptr(NULL), kb_sent(0), p(0), maxr(0), dial(0), downloadSpeedFactor(0), uri(""), starttime(""), filelength(0), bytecount(0), secondElapsed(0) { }
    Connection(int c, int s, string stri, shared_ptr<thread> p) { conn_number = c; socket_fd = s; ip_and_port_info = stri; thread_ptr = p; kb_sent = 0; }
};

int connection_num = 1;
vector<shared_ptr<thread> > thread_vector; // vector of shared pointers to threads
double downloadSpeedFactor = 1;
int active_connections = 0;

mutex m_;
vector<shared_ptr<Connection> > connection_list;

/**
 * Use this code to return the file size of path.
 *
 * You should be able to use this function as it.
 *
 * @param path - a file system path.
 * @return the file size of path, or (-1) if failure.
 */
static
int get_file_size(string path)
{
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0) {
        return (-1);
    }
    return (int)(stat_buf.st_size);
}

static
string HexDump(unsigned char *buf, int len)
{
    string s;
    static char hexchar[]="0123456789abcdef";

    for (int i=0; i < len; i++) {
        unsigned char ch=buf[i];
        unsigned int hi_nibble=(unsigned int)((ch>>4)&0x0f);
        unsigned int lo_nibble=(unsigned int)(ch&0x0f);

        s += hexchar[hi_nibble];
        s += hexchar[lo_nibble];
    }
    return s;
}

bool isNumber(const string& s)
{
    for (char const &ch : s) {
        if (std::isdigit(ch) == 0) 
            return false;
    }
    return true;
 }


void read_ini_file(const char* ini_file){
    int fd = open(ini_file, O_RDONLY);
    if(fd < 0){
        perror("Error when opening file");
        exit(0);
    }

    map<string, map<string, string> > iniTable;
    map<string, string> currentSectionMap;
    bool initialized = false;
    string currentSection; 
    string lastSection;
    string keyStr;
    string valueStr;
    while(true){
        string line;
        read_a_line(fd, line);
        if(line[0] == ';' ){
            continue;
        }
        if(line.size() == 0 || line == "\n\n"){
            break;
        }
        if(line[0] == '['){
            if(initialized == true){
                iniTable.emplace(lastSection, currentSectionMap);
                currentSectionMap.clear();
            }
            currentSection = line.substr(1, line.length() - 3);
            initialized = true;
        }
        else{
            int found = line.find("=");
            keyStr = line.substr(0, found);
            valueStr = line.substr(found+1, line.length());
            unsigned int pos = valueStr.find("\n");
            int p;
            int maxr;
            int dial;
            if (pos != string::npos)
             {
                 // If found then erase it from string
                 valueStr.erase(pos, string("\n").length());
            }

            if(currentSection == "startup"){
                if(keyStr == "port"){
                    port = valueStr;
                }
                else if(keyStr == "logfile"){
                    logFile = valueStr;
                }
                else if(keyStr == "pidfile"){
                    pid_file = valueStr;
                }
                else if(keyStr == "rootdir"){
                    rootdir = valueStr;
                }
            }
            else{
                if(keyStr == "P"){
                    p = stoi(valueStr);
                }
                else if(keyStr == "MAXR"){
                    maxr = stoi(valueStr);
                }
                else if(keyStr == "DIAL"){
                    dial = stoi(valueStr);
                    double spd = ((double)maxr/(double)p)*((double)dial/100);
                    vector<int> tmp;
                    tmp.push_back(p);
                    tmp.push_back(maxr);
                    tmp.push_back(dial);
                    
                    throttleMap.emplace(currentSection, spd);
                    throttle_vector_map.emplace(currentSection, tmp);
                }
            }
            currentSectionMap.emplace(keyStr, valueStr);
            lastSection = currentSection;
            }
        }


    if(rootdir.empty() ||rootdir.empty() ||  port.empty()){
        cerr << "Incomplete information for server!" << endl;
        exit(1);
    }
}

static
void Init()
{
    if (!logFile.empty()) {
        myfile.open(logFile, ofstream::out|ofstream::app);
        mylog = &myfile;
    } else {
        mylog = &cout;
    }
}

static
void CleanUp()
{
    if (!logFile.empty()) {
        myfile.close();
        // cout << "Some messages were written into file '" << logFile << "' (and this message is written to cout)" << endl;
    } else {
        // cout << "All messages were written into cout (includingn this message)" << endl;
    }
}

string md5_calculator(string filepath){
    ifstream myfile;

    myfile.open(filepath, ifstream::in|ios::binary);
    if (myfile.fail()) {
        cerr << "Cannot open '" << filepath << "' for MD5 reading." << endl;
        exit(-1);
    }
    int bytes_remaining = get_file_size(filepath);
    MD5_CTX md5_ctx;

    MD5_Init(&md5_ctx);
    while (bytes_remaining > 0) {
        char buf[0x1000]; /* 4KB buffer */

        int bytes_to_read = ((bytes_remaining > (int)sizeof(buf)) ? sizeof(buf) : bytes_remaining);
        myfile.read(buf, bytes_to_read);
        if (myfile.fail()) {
            break;
        }
        MD5_Update(&md5_ctx, buf, bytes_to_read);
        bytes_remaining -= bytes_to_read;
    }
    myfile.close();
    unsigned char md5_buf[MD5_DIGEST_LENGTH];

    MD5_Final(md5_buf, &md5_ctx);

    string md5 = HexDump(md5_buf, sizeof(md5_buf));
    return md5;
    
}

/*
 * This is the same idea as LogALine().  The only difference is that you always use the mylog global variable.
 */
static
void LogALineVersion(string a_line_of_msg)
{
    Init();
    *mylog << a_line_of_msg;
    mylog->flush();
    CleanUp();
}


/**
 * Open in_filename_string for reading.
 *
 * @param in_filename_string - file name to open for reading.
 * @return (-1) if file cannot be opened; otherwise, return a file descriptor.
 */
int open_file_for_reading(string in_filename_string)
{
    return open(in_filename_string.c_str(), O_RDONLY);
}

/**
 * Open out_filename_string for writing (create the file if it doesn't already exist).
 *
 * @param out_filename_string - file name to open for writing.
 * @return (-1) if file cannot be opened; otherwise, return a file descriptor.
 */
int open_file_for_writing(string out_filename_string)
{
    int fd = open_file_for_reading(out_filename_string);
    if (fd == (-1)) {
        fd = open(out_filename_string.c_str(), O_WRONLY|O_CREAT, 0600);
    } else {
        close(fd);
        fd = open(out_filename_string.c_str(), O_WRONLY|O_TRUNC);
    }   
    return fd;
}

// SERVER
static bool isInvalidHeader(string line){
    return false;
}

// SERVER
static int process_header_and_send_response(shared_ptr<Connection> conn_ptr, vector<string> request, int newsockfd, string ip_and_port_info, int process_num){

    string substr = "";
    vector<string> request_header;
    for(unsigned int i = 0; i < request[0].size(); i++){
        if(request[0][i] == ' '){
            request_header.push_back(substr);
            substr = "";
        }      
        else{
            substr = substr + request[0][i];
            }
    }
    // Search for the substring in string
    unsigned int pos = substr.find("\r\n");
    if (pos != string::npos)
    {
        // If found then erase it from string
        substr.erase(pos, string("\r\n").length());
    }
    request_header.push_back(substr);


    // validate the header
    if(request_header.size() != 3 || request_header[0] != "GET" || 
     request_header[1][request_header[1].length()-1] == '/' || request_header[1].find('?') != string::npos ||
      request_header[1].find('#') != string::npos){

        //#############
        string logmessage = "[" + get_timestamp_now() + "] REQUEST[" + to_string(conn_ptr->conn_number) + "]: " +  ip_and_port_info + ", uri="+ request_header[1] + "\n";
        for(unsigned int j = 0; j < request.size(); j++){
            logmessage += "\t[" + to_string(conn_ptr->conn_number) + "]\t" + request[j];
        }
        LogALineVersion(logmessage);

        string resultingStr = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 63\r\nContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";
        better_write(newsockfd, resultingStr.c_str(), resultingStr.length());
        //#############
        logmessage = "[" + get_timestamp_now() + "] RESPONSE[" + to_string(conn_ptr->conn_number) + "]" + ip_and_port_info + ", status=404\n";
        LogALineVersion(logmessage);
      }
      else{
        // //#############
        // string logmessage = "[" + get_timestamp_now() + "] REQUEST[" + to_string(conn_ptr->conn_number) + "]: " +  ip_and_port_info + ", uri="+ request_header[1] + "\n";
        // for(unsigned int j = 0; j < request.size(); j++){
        //     logmessage += "\t[" + to_string(conn_ptr->conn_number) + "]: " + request[j];
        // }
        conn_ptr->uri = request_header[1];

        int found = request_header[1].find(".");
        string fileExtension = request_header[1].substr(found+1, request_header[1].length());
        map<string,double>::iterator it = throttleMap.find(fileExtension);
        
        if(it != throttleMap.end())
        {
            downloadSpeedFactor = it->second;
            map<string,vector<int> >::iterator it = throttle_vector_map.find(fileExtension);
            vector<int> throttle_info = it->second;
            conn_ptr->p = double(throttle_info[0]);
            conn_ptr->maxr = double(throttle_info[1]);
            conn_ptr->dial = double(throttle_info[2]);  
            conn_ptr->downloadSpeedFactor = (conn_ptr->maxr/conn_ptr->p) * (conn_ptr->dial/100);
        }
        else{
            it = throttleMap.find("*");
            downloadSpeedFactor = it->second;
            map<string,vector<int> >::iterator it = throttle_vector_map.find("*");
            vector<int> throttle_info = it->second;
            conn_ptr->p = double(throttle_info[0]);
            conn_ptr->maxr = double(throttle_info[1]);
            conn_ptr->dial = double(throttle_info[2]);  
            conn_ptr->downloadSpeedFactor = (conn_ptr->maxr/conn_ptr->p) * (conn_ptr->dial/100);
        }

        //#############
        string logmessage = "[" + get_timestamp_now() + "] REQUEST[" + to_string(conn_ptr->conn_number) + "]: " +  ip_and_port_info + ", uri="+ request_header[1] + "\n";
        for(unsigned int j = 0; j < request.size(); j++){
            logmessage += "\t[" +to_string(conn_ptr->conn_number) +  "]\t" + request[j];
        }
        logmessage += "\t[" +to_string(conn_ptr->conn_number) + "]\n";
        LogALineVersion(logmessage);
        string path =rootdir + request_header[1];
        if(get_file_size(path) != -1){
            logmessage = "[" + get_timestamp_now() + "] RESPONSE[" + to_string(conn_ptr->conn_number) + "]: " +  ip_and_port_info + ", status=200\n";
            string file_md5 = md5_calculator(path);
            string resultingStr;
            string prefix = "\t[" + to_string(conn_ptr->conn_number) + "]\t";
            resultingStr += string(request_header[2]) + " 200 OK" + "\r\n";
            logmessage += prefix + request_header[2] + " 200 OK" + "\r\n";
            resultingStr += "Server: pa3 (yimingx@usc.edu)\r\n";
            logmessage += prefix + "Server: pa3 (yimingx@usc.edu)\r\n";

            unsigned int findHtml = request_header[1].find(".htm");
            if (findHtml != string::npos){
                resultingStr += "Content-Type: text/html\r\n";
                logmessage += prefix + "Content-Type: text/html\r\n";
            }
            else{
                resultingStr += "Content-Type: application/octet-stream\r\n";
                logmessage += prefix + "Content-Type: application/octet-stream\r\n";
            }
            resultingStr += "Content-Length: " + to_string(get_file_size(path)) + "\r\n";
            logmessage += prefix + "Content-Length: " + to_string(get_file_size(path)) + "\r\n";
            
            conn_ptr->filelength = get_file_size(path);
            resultingStr += "Content-MD5: " + file_md5 + "\r\n";
            logmessage += prefix + "Content-MD5: " + file_md5 + "\r\n";
            resultingStr += "\r\n";
            logmessage += prefix + "\r\n";

            // SEND RESPONSE    
            LogALineVersion(logmessage);

            // SEND RESPONSE    
            // write response header and blank line into socket_fd
            conn_ptr->starttime = get_timestamp_now();
            struct timeval starttime;
            gettimeofday(&starttime, NULL);
            better_write(newsockfd, resultingStr.c_str(), resultingStr.length());
            // read the file
            int fd = open(path.c_str(), O_RDONLY);
            if(fd < 0){
                string resultingStr = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 63\r\nContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";
                better_write(newsockfd, resultingStr.c_str(), resultingStr.length());
                //#############
                string logmessage = "[" + get_timestamp_now() + "] RESPONSE[" + to_string(conn_ptr->conn_number) + "]" + ip_and_port_info + ", status=404\n";
                LogALineVersion(logmessage);
            }     
            else{
                //#############
                stringstream tmp;
                tmp << setprecision(3) << fixed << (conn_ptr->maxr / conn_ptr->p)*(conn_ptr->dial/100);
                double new_val = stod(tmp.str());   // new_val = 3.143
                tmp.str(string());  
                string logmessage = "[" + get_timestamp_now() + "] RESPONSE[" + to_string(conn_ptr->conn_number) + "]" + ip_and_port_info + ", status=200, P=" + to_string(int(conn_ptr->p)) + ", MAXR=" + to_string(int(conn_ptr->maxr)) + " tokens/s, DIAL=" +  to_string(int(conn_ptr->dial)) + "%, rate=" + to_string(new_val) + " KB/s\n";
                LogALineVersion(logmessage);
                
                int fileSize = get_file_size(path);
                int time_calling_betterwrite = 0;
                // P = 1
                int P = 1;
                // r = SPEED /* from argv[2] */
                //downloadSpeedFactor = (MAXR/P)×(DIAL/100) 
                double r = conn_ptr->downloadSpeedFactor;
                // t1 = gettimeofday()
                int b1 = P;
                struct timeval t1;
                gettimeofday(&t1, NULL);
                struct timeval t2;
                struct timeval t3;
                double time_to_sleep;
                // b1 = P
                // data = read 1024 bytes from file
                bool not_enough_tokens = true;
                char character[1024];
                int strLen = read(fd, character, 1024);
                conn_ptr->bytecount = double(conn_ptr->kb_sent) / fileSize;
                struct timeval currenttime;
                while(strLen > 0){
                    gettimeofday(&currenttime, NULL);
                    conn_ptr->secondElapsed = timestamp_diff_in_seconds(&currenttime, &starttime);
                    // write(socket_fd, data)
                    better_write(newsockfd, character, strLen);
                    
                    not_enough_tokens = true;
                    time_calling_betterwrite++;

                    m_.lock();
                    conn_ptr->kb_sent++;
                    if(conn_ptr->socket_fd == -2 || master_socket_fd == -1){
                        m_.unlock();
                        return false;
                    }
                    m_.unlock();

                    while (not_enough_tokens) {
                        gettimeofday(&t2, NULL);
                        int n = (int)(r * (timestamp_diff_in_seconds(&t1, &t2)));/* must truncate and not round() */
                        if ((n > 1) || (b1 == P && b1-P+n >= P) || (b1 < P && b1+n >= P)){
                            add_seconds_to_timestamp(&t1, (double(1.0)/double(r)), &t1);
                            b1 = P;
                            not_enough_tokens = false;
                        }
                        else{
                            /* since P is 1, we must have n == 0 and b1 == 1 here */
                            b1 = 0;        /* b1 = b1-P+n */
                            add_seconds_to_timestamp(&t1, (double(1.0)/double(r)), &t3);
                           // (struct timeval *older, double d_seconds, struct timeval *newer_return)
                            time_to_sleep =  timestamp_diff_in_seconds(&t2,&t3);
                            double usec_to_sleep = time_to_sleep * 1000000;
                            if (usec_to_sleep > 0) {
                                if (usec_to_sleep > 250000){
                                    usleep(250000);
                                }
                                else{
                                    usleep(usec_to_sleep);
                                }
                                // usleep(usec_to_sleep);
                            }
                        }
                    }

                    m_.unlock();
                    if(conn_ptr->socket_fd == -2 || master_socket_fd == -1){
                        m_.unlock();
                        return false;
                    }
                    m_.unlock();
                    // usleep(1000000/downloadSpeedFactor);
                    // cout after send 1KB of the response body to the client and before you call usleep()
                    // string currentLog = "[" + get_timestamp_now() + "] [" + to_string(process_num) + "]\tSent " + to_string(time_calling_betterwrite) + " KB to " + ip_and_port_info + "\n";
                
                    // LogALineVersion(currentLog);
                    strLen = read(fd, character, 1024);
                    fileSize-= strLen;
                }
                close(fd);
            } 
        
        }
        else{
                string resultingStr = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 63\r\nContent-MD5: 5b7e68429c49c66e88489a80d9780025\r\n\r\n<html><head></head><body><h1>404 Not Found</h1></body></html>\r\n";
                better_write(newsockfd, resultingStr.c_str(), resultingStr.length());
            }
        }

        // print this before talk_to_client returns
        // string currentLog = "[" + get_timestamp_now() + "] [" + to_string(process_num) + "]\tConnection closed with client " +  to_string(conn_ptr->conn_number) + " at " + ip_and_port_info + "\n";
        // LogALineVersion(currentLog);
        return true;
      }
      



// SERVER
/**
 * This is the function you need to change to change the behavior of your server!
 *
 * @param newsockfd - socket that can be used to "talk" (i.e., read/write) to the client.
 */
static
// void talk_to_client(int newsockfd, string ip_port_info, int process_num)
void talk_to_client(shared_ptr<Connection> conn_ptr)
{
    /* Connection is incomplete */
    m_.lock();
    active_connections++;
    m_.unlock();
    conn_ptr->orig_socket_fd = conn_ptr->socket_fd;
    /* Connection is now complete */
    // do forever
    bool not_closed = true;
    while(not_closed){
        // READ REQUEST
        // read header from client_socket
        string line;
        int bytes_received = read_a_line(conn_ptr->socket_fd, line);
        // if (header is empty) break
        if(bytes_received <=0) break;
        else if(isInvalidHeader(line)){
            // send error response
            // TODO: THIS IS TEMPORARY OUTPUT MESSAGE
            cout << "There are some error with the header" << endl;
            // break
            exit(1);
        }
        // if the header is valid
        else{
            vector<string> request;
            while(line != "\r\n"){
                request.push_back(line);
                read_a_line(conn_ptr->socket_fd, line);
            }
            // process header and send response
            not_closed = process_header_and_send_response(conn_ptr, request, conn_ptr->socket_fd, conn_ptr->ip_and_port_info, conn_ptr->conn_number);
        }

    }
    // shutdown and close client_socket
    m_.lock();
    if(conn_ptr->socket_fd >= 0){
        shutdown(conn_ptr->socket_fd, SHUT_RDWR);
    }
    conn_ptr->socket_fd = -1;
    close(conn_ptr->orig_socket_fd);
    active_connections--;
    m_.unlock();
    //###########
    string currentLog = "[" + get_timestamp_now() + "] CLOSE[" + to_string(conn_ptr->conn_number) + "]: (done) " + conn_ptr->ip_and_port_info + "\n";
    LogALineVersion(currentLog);
}


// SERVER
static void runAsServer(const string port_number_string){
    // 1. parse commandline arguments
    // 2. listening_socket = initialize and create listening socket
    master_socket_fd = create_master_socket(port_number_string);

    // validate the socket number
    if (master_socket_fd == (-1)) {
         cout << "no master socket found" << endl;
            shutdown(master_socket_fd, SHUT_RDWR);
            close(master_socket_fd);
    }
        // string s = get_ip_and_port_for_server(master_socket_fd, 0);
        // do-forever
        // ###########
        string currentLog = "[" + get_timestamp_now() + "] START: port=" + port_number_string + ", rootdir=\'" + rootdir + "\'\n";
        LogALineVersion(currentLog);
        for (;;) {
            // client_socket = accept(listning_socket)
            int newsockfd = my_accept(master_socket_fd);


            // if(client_socket is valid) then
            if(newsockfd == -1){
                return;
            }
            m_.lock();
            if(master_socket_fd == -1){
                shutdown(master_socket_fd, SHUT_RDWR);
                close(master_socket_fd);
                m_.unlock();
                return;
            }
            if (newsockfd != (-1)){
                if(connection_list.size() == 999999 ){
                    cout << "999,999 connections served.  Proceed with auto-shutdown..." << endl;
                     m_.lock();
                    shutdown(master_socket_fd, SHUT_RDWR);
                    close(master_socket_fd);
                    master_socket_fd = -1;
                    for(int i = 0; i < connection_list.size(); i++){
                        if(connection_list[i]->socket_fd >= 0){
                            shutdown(connection_list[i]->socket_fd, SHUT_RDWR);
                            connection_list[i]->socket_fd = -2;
                            currentLog = "[" + get_timestamp_now() + "] CLOSE[" + to_string(connection_list[i]->conn_number) + "]: (unexpected)" + connection_list[i]->ip_and_port_info + "\n";
                            LogALineVersion(currentLog);
                        }
                    }
                    m_.unlock();
                    return;
                }
                // talk to client
                // ####################
                string s = get_ip_and_port_for_server(master_socket_fd, 0);
                currentLog = "[" + get_timestamp_now() + "] CONNECT[" + to_string(connection_num) + "]: "+ s + "\n";
                LogALineVersion(currentLog);

                shared_ptr<Connection> conn_ptr = make_shared<Connection>(Connection(connection_num++, newsockfd, s, NULL));
                shared_ptr<thread> thr_ptr = make_shared<thread>(thread(talk_to_client, conn_ptr));
                conn_ptr->thread_ptr = thr_ptr;
                connection_list.push_back(conn_ptr);


                m_.unlock();
            }
        }
    
}


void console(){
    while(true){
        cout << "> ";
        string user_input;
        cin >> user_input;
        
        if(user_input.substr(0,5) == "close"){

            string requested_connection_str;
            cin >> requested_connection_str;
            
            int requested_connection = stoi(requested_connection_str);
            m_.lock();
            // shutdown and close client_socket
            bool found = false;
            if(requested_connection <= connection_list.size()){
                for(int i = 0; i < connection_list.size(); i++){
                    if(connection_list[i]->conn_number == requested_connection){
                        if(connection_list[i]->socket_fd >= 0){
                            found = true;
                            shutdown(connection_list[i]->socket_fd, SHUT_RDWR);
                            connection_list[i]->socket_fd = -2;
                            // ####
                            string currentLog = "[" + get_timestamp_now() + "] CLOSE[" + to_string(connection_list[i]->conn_number) + "]: (at user's request) " + connection_list[i]->ip_and_port_info + "\n";
                            LogALineVersion(currentLog);
                            cout << "Closing connection " << requested_connection << "..." << endl;
                            active_connections--;
                        }
                    }
                }
                if(found == false){
                    cout << "No such connection: " << requested_connection << endl;
                }
            }
            m_.unlock();
            
        }
        else if(user_input == "dial"){
            string requested_connection_str;
            cin >> requested_connection_str;

            string percent;
            cin >> percent;
            

            if(!isNumber(percent)){
                cout << "Invalid percent.  The command syntax is \"dial # percent\".  Please try again." << endl;
                continue;
            }
            if(stoi(percent) < 1 || stoi(percent) > 100){
                cout << "Invalid percent.  The command syntax is \"dial # percent\".  Please try again." << endl;
                continue;
            }
            else{
                bool found = false;
                for(int i = 0; i < connection_list.size(); i++){
                    if(connection_list[i]->conn_number == stoi(requested_connection_str)){
                        found = true;
                        cout << "Dial for connection " + to_string(connection_list[i]->conn_number) + " at " + percent + "%.  Token rate at ??? tokens/s.  Data rate at ??? KB/s." << endl;
                        connection_list[i]->dial = stoi(percent);
                    }
                }
                if(!found){
                    cout << "No such connection: " << requested_connection_str << endl;
                }
            }
        }
        else if(user_input == "status"){
            m_.lock();
            string no_connection_str("No active connection.\n");
            string active_connections("The following connections are active:\n");
            
            int active_connection_count = 0;
            for(int i = 0; i < connection_list.size(); i++){
                if(connection_list[i]->socket_fd >= 0){
                    active_connections += "[" + to_string(connection_list[i]->conn_number) + "]\tClient at " + connection_list[i]->ip_and_port_info + "\n";
                    active_connections += "\tPath: " + 
            Path: URI\n
            Content-Length: LENGTH\n
            Start-Time: [TIMESTAMP]\n
            Shaper-Params: P=???, MAXR=??? tokens/s, DIAL=???%, rate=??? KB/s\n
            Sent: BC bytes (F%), time elapsed: ET sec\n
                    active_connection_count++;
                }
            }
            if(active_connection_count == 0){
                cout << no_connection_str << endl;
            }
            else{
                cout << active_connections << endl;
            }
            m_.unlock();
        }
        else if(user_input == "quit"){
            break;

        }
        else{
            cout << "Command not recognized.  Valid commands are:\n\tclose #\n\tdial # percent\n\tquit\n\tstatus" << endl;
        }
    }
    m_.lock();
    shutdown(master_socket_fd, SHUT_RDWR);
    close(master_socket_fd);
    master_socket_fd = -1;
    for(int i = 0; i < connection_list.size(); i++){
        if(connection_list[i]->socket_fd >= 0){
            shutdown(connection_list[i]->socket_fd, SHUT_RDWR);
            connection_list[i]->socket_fd = -2;
            //###########
            string currentLog = "[" + get_timestamp_now() + "] CLOSE[" + to_string(connection_list[i]->conn_number) + "]: (at user's request)" + connection_list[i]->ip_and_port_info + "\n";
            LogALineVersion(currentLog);
        }
    }
    m_.unlock();
    cout << "Console thread terminated" << endl;
    // #####
    string currentLog = "[" + get_timestamp_now() + "] STOP: port=" + port + "\n";
    LogALineVersion(currentLog);
    return;
}

void reaper(){
    //     do forever
    while(true){
        usleep(250000);
        m_.lock();
        if(master_socket_fd == -1 && connection_list.size() == 0){
            m_.unlock();
            break;
        }
        else{
            for (vector<shared_ptr<Connection> >::iterator itr = connection_list.begin(); itr != connection_list.end(); ) {
                shared_ptr<Connection> connection_ptr = (*itr);
                if (connection_ptr->socket_fd == -1) {
                    connection_ptr->thread_ptr->join();
                    // string currentLog = "[" + get_timestamp_now() + "] [" + to_string(connection_ptr->conn_number) + "]\tReaper has joined with connection thread\n";
                    // LogALineVersion(currentLog);
                    itr = connection_list.erase(itr);
                } 
                else 
                {
                    itr++;
                }
            }
        }
        m_.unlock();
    }
}

int main(int argc, char *argv[])
{
    // log required message
    // create console thread
    shared_ptr<thread> connection_thread = make_shared<thread> (thread(console)); 
    shared_ptr<thread> reaper_thread = make_shared<thread> (thread(reaper)); 
    downloadSpeedFactor = 1;
    // logFile = argv[2];
    // string portNumber = string(argv[1]);
    read_ini_file(argv[1]);
    // downloadSpeedFactor = ;
    // (MAXR/P)×(DIAL/100) 
    runAsServer(port);

    connection_thread->join();
    reaper_thread->join();
    string currentLog = "[" + get_timestamp_now() + "] Main thread closed.\n";
    // LogALineVersion(currentLog);

    return 0;
}
