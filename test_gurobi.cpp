#include "gurobi_c++.h"
#include <iostream>

int main() {
    try {
        GRBEnv env = GRBEnv();
        GRBModel model = GRBModel(env);
        std::cout << "Gurobi C++ API funcionando correctamente." << std::endl;
    } catch (GRBException e) {
        std::cerr << "Error: " << e.getMessage() << std::endl;
        return 1;
    }
    return 0;
}

