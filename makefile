codegen: parse.cpp lower.cpp ast.cpp codegen.cpp
	g++ -std=c++11 -Wall parse.cpp lower.cpp ast.cpp codegen.cpp -o codegen
clean:
	rm -f codegen