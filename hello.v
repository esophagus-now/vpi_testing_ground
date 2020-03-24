module main;
reg clk = 0;

reg [31:0] cnt = 0;
reg [5:0] ev_cnt = 0;
reg [5:0] call_cnt = 0;

event test_event;

reg [15:0] tdata = 16'd5555;
reg tvalid = 1;


initial begin
	$dumpfile("test.vcd");
	$dumpvars;
	$dumplimit(512000);
	$display("cnt = %d", cnt);
	repeat (5) begin
		$my_task(tdata, tvalid, 0);
		call_cnt = call_cnt + 1;
	end
	
	$display("cnt = %d", cnt);
	#100
	$display("cnt = %d", cnt);
	$finish;
end

always #5 clk <= ~clk;

always @(posedge clk) begin
	cnt <= cnt + 1;
end

always @(test_event) begin
	$display("Event fired, ev_cnt = %d", ev_cnt);
	ev_cnt <= ev_cnt + 1;
end

endmodule
