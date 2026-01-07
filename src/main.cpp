#include "headers/main.hpp"


int main() {
    renderer Renderer = renderer(800, 600);

    if (!Renderer.checkStatus()) {
        return 1;
    } 

    return Renderer.loop();
}
