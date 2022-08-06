#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace std;

int main(){
    stringstream ss;
    string s="afaf sdfa sf , afdf45a            ";
    ss<<s;
    do{
        ss>>s;
        cout<<s<<"\n";
    }while(!ss.eof());
}