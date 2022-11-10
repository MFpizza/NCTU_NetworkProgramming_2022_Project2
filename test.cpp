#include <map>
#include <iostream>
using namespace std;

int main(){
    map<int, map<int, int *>> userpipe;
    map<int, int *> newChildMap;
    newChildMap[0]= new int[2]();
    userpipe[0] = newChildMap;
    cout<<userpipe[0].count(1)<<endl;
    cout<<userpipe[0].count(0)<<endl;
    cout<<userpipe[0][0]<<endl;
    cout<<userpipe.count(1)<<endl;
    cout<<userpipe.count(0)<<endl;
    int *u = userpipe[0][0];
    cout<<u[0]<<endl;
}