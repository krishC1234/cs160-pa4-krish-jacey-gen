codegen: parse.cpp lower.cpp ast.cpp 
	g++ -std=c++11 -Wall parse.cpp lower.cpp ast.cpp -o codegen
clean:
	rm -f codegen