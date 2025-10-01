#include <iostream>
import hello;

int main() {
    say_hello();
    std::cout<<"main.cpp"<<std::endl;
    std::cout<<retv()<<std::endl;
    std::cout<<get_greeting()<<std::endl;
    return 0;
}
