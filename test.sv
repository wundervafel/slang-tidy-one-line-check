module top ();
    logic a, b, c;
    always_ff @(posedge c) begin
        int k; a = b;
    end
endmodule


module top1 ();
    initial begin
        int m, n; int h;
        int k = 1, l = 2; k = 42;

    end
endmodule

module top2 (output m, output n);
    logic a = 2, b = 3, c = 4;

    reg f, g;
    assign f = a | b, g = a & b;


    and (m, b, c); nand (n, b, a);
endmodule

module top3 ();
    logic a, b, c;
    always @(posedge a) begin
        a = b; c = a;

        if (a) b = a; else c = b;

        if (a) b = 1;
        else c = b;
    end

    logic d;
endmodule

module top4 ();
    logic a, b, c;
    always_ff @(posedge a) begin
        case (a) b: a = b;
            1: a = b; 2: a = c;
            3: b = c; default a = 1;
        endcase
    end
endmodule

module top6 ();
    logic a, b, c; int k = 8;
    always @(posedge a) begin
        while (k > 0) k = k - 1;
    end
endmodule

`define macro1(a) assign a = a;
`define macro2(b) b = 1;

module top7 ();
    logic a, b, c;
    int k = 8;
    always @(posedge a) begin
        while (k > 0) k = k - 1;

        `macro1(a) `macro2(b)
    end

    always #10 begin : name
        while (k > 0) k = k - 1;

        `macro2(b) `macro1(a)

    end

    // Найти assign внутри символов пока не удалось,
    // Поэтому на него чекер не действует
    int m, n, d; assign m = n | d, n = d + m;
    int j, r, l;
    assign j = l | l, l = r + j;

    initial repeat (10) a = 2;
endmodule

module top8 ();
    logic a, b, c;
    int k; always @(posedge a) begin
        for (int p = 1; k < 5; k = k + 1) a = a + 1;

        for (int k = 1; k < 5; k = k + 1) begin
            a = a + k;
        end

        for (int p = 1; k < 5; k = k + 1) begin a = a + k;
        end

        for (int k = 1; k < 5; k = k + 1) a = a + k; b = 1;
    end
endmodule

