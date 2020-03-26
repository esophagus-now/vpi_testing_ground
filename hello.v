`timescale 1ns / 1ps

module main;
reg clk = 0;

reg [15:0] tdata = 16'd5555;
reg tvalid = 1;
wire tready;

initial begin
	$dumpfile("test.vcd");
	$dumpvars;
	$dumplimit(512000);	
	#600
	$finish;
end

always #5 clk <= ~clk;

reg is_ready = 0;
reg [31:0] tmp = 0;

always @(posedge clk) begin
	$my_task(tdata, tvalid, tready);
	tmp <= $random;
	is_ready <= tmp[0];
end

assign tready = is_ready;

endmodule
