declare i8** @function2();

define void @function3() {
    %1 = call i8** @function2()
    %2 = getelementptr i8*, i8** %1, i32 1
    %string = load i8*, i8** %2

    ret void
}
