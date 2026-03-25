#include "TidyTest.h"

TEST_CASE("NoOneLineMultiAssign: Two assignments on the same line") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial begin
        c = a + b; d = a - b;
    end
endmodule
)");
    CHECK(result);
}

TEST_CASE("NoOneLineMultiAssign: Another two assignments on the same line") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    assign c = a | b, d = a & b;
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: Else branch on the same line as if branch") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial begin
        if (a) c = a; else d = b;
    end
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: Else branch on the different line from if branch") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial begin
        if (a) c = a;
        else d = b;
    end
endmodule
)");
    CHECK(result);
}

TEST_CASE("NoOneLineMultiAssign: Statement on the same line as block beginning") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial begin c = a;
    end
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: Statement on the different line from block beginning") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial begin
        c = a;
    end
endmodule
)");
    CHECK(result);
}

TEST_CASE("NoOneLineMultiAssign: Statement on the same line as timing control") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    allways @(posedge a) c = a;
endmodule
)");
    CHECK(result);
}

TEST_CASE("NoOneLineMultiAssign: Statement on the same line block beginning after timing control") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    allways @(posedge a) begin c = a;
    end
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: Statement inside block after timing control") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    allways @(posedge a) begin
        c = a;
    end
endmodule
)");
    CHECK(result);
}

TEST_CASE("NoOneLineMultiAssign: Two assignment on the same line inside a block") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    allways @(posedge a) begin
        int k;
        c = a;
        k = a + b; d = b;
    end
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: Conditional statement on the same line as assignment") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    allways @(posedge a) begin
        d = c; if (a) c = a;
        else d = b;
    end
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: Two cases on the same line") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial begin
        case (a)
            b: c = d; 1: d = c;
        endcase
    end
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: Default case on the same line with another case") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial
    case (a)
        b: c = d; default: d = c;
    endcase
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: Another default case on the same line with another case") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial
    case (a)
        default: c = d; b: d = c;
    endcase
endmodule
)");
    CHECK_FALSE(result);
}

TEST_CASE("NoOneLineMultiAssign: All cases on the different lines") {
    auto result = runCheckTest("NoOneLineMultiAssign", R"(
module top (input a, input b, output c, output d);
    initial
    case (a)
        1: d = b;
        default: c = d;
        b: d = c;
    endcase
endmodule
)");
    CHECK(result);
}
