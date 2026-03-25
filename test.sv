module top ();
    logic a, b, c;
    always_ff @(posedge c) begin
        int k; a = b;
    end
endmodule


module top1 ();
    initial begin
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
endmodule

module top4 ();
    logic a, b, c;
    always_ff @(posedge a) begin
        // Почему то если case первый в блоке, то блок начинается с той
        // строчки, с которой начинается case
        case (a)
            b: a = b;
            1: a = b; 2: a = c;
            3: b = c; default a = 1;
        endcase
    end
endmodule

module top5 ();
    logic a, b, c;
    int k;
    always @(posedge a) begin
        for (int k = 1; k < 5; k = k + 1) a = a + 1;

        for (int k = 1; k < 5; k = k + 1) begin
            a = a + k;
        end

        for (int k = 1; k < 5; k = k + 1) begin a = a + k;
        end

        for (int k = 1; k < 5; k = k + 1) a = a + k; b = 1;
    end
endmodule

module top6 ();
    logic a, b, c;
    int k = 8;
    always @(posedge a) begin
        while (k > 0) k = k - 1;
    end
endmodule
