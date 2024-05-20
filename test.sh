~benh/160/lex ts_raw > ts_token
~benh/160/parse -json ts_token > ts_ast
~benh/160/lower -hr ts_ast > ts_lower
~benh/160/codegen ts_lower > ts_correct

make clean
make
./codegen ts_token ts_token ts_token > ts_ours
