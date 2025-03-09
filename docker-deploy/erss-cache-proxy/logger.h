#include<string>
#include<iostream>
#include<mutex>
#include<fstream>
using namespace std;
#ifndef LOGGER_H
#define LOGGER_H
class Logger{
public:
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    void log(const string &log_info){
        //lock
        lock_guard<mutex> guard(mutex_lock_);
        //write log
        logFile << log_info << endl;

    }
    Logger(const string &output_path):logFile(output_path){
            
    }
    ~Logger() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }


private:
    
    mutable mutex mutex_lock_;
    ofstream logFile;
};
#endif