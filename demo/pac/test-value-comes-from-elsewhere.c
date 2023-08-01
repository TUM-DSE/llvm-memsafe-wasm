#include<stdlib.h>

// === TEST-CASE 1:

// gets memory location, to be loaded from, from parameter, our llvm should detect not to auth here on load
void function(char **double_ptr) {
    *double_ptr;
}

// Equivalent llvm-ir:
/*
define void @function(i8** %double_ptr) {
  %1 = load i8*, i8** %double_ptr
  ret void
}
*/


// === TEST-CASE 2:

char **function2() {
    char *string = malloc(42 * sizeof(char));
    char **double_ptr = malloc(sizeof(char *));
    // store pointer
    *double_ptr = string;
    return double_ptr;
}

// gets memory location, to be loaded from, from other function's return value directly, our llvm should detect not to auth here on load
void function3() {
    char **string = function2();
    // load pointer
    char *loaded_ptr = *string;
    free(*string);
    free(string);
}

// Equivalent llvm-ir:
/*
declare i8** @function2();

define void @function3() {
    %1 = call i8** @function2()
    %string = load i8*, i8** %1

    ret void
}
*/


// === TEST-CASE 3:

// gets memory location, to be loaded from, from other function's return value with some slight code in between, our llvm should detect not to auth here on load
void function3() {
    char **string = function2();
    string++;
    char *loaded_ptr = *string;
}

// Equivalent llvm-ir:
/*
declare i8** @function2();

define void @function3() {
    %1 = call i8** @function2()
    %2 = getelementptr i8*, i8** %1, i32 1
    %string = load i8*, i8** %2

    ret void
}
*/

// === TEST-CASE 4:

// gets memory location, to be loaded from, from function parameter with some slight code in between, our llvm should detect not to auth here on load
void function3(char **string) {
    string++;
    char *loaded_ptr = *string;
}

// Equivalent llvm-ir:
/*
define void @function3(i8** %string) {
    %1 = getelementptr i8**, i8*** %string, i32 1
    %2 = load i8**, i8*** %1
    %loaded_ptr = load i8*, i8** %2

    ret void
}
*/

// === TEST-CASE 5:

void main(int argc, char **argv) {
    // load pointer => comes from elsewhere (i.e. function parameter), so we shouldn't authenticate it
    argv[0];
}

// Equivalent llvm-ir:
/*
define i32 @main(i32 %argc, i8** %argv) {
    %1 = getelementptr i8*, i8** %argv, i32 0
    %loaded_ptr = load i8*, i8** %1

    ret i32 0
}
*/
