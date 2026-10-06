
module router_top(clock,resetn,pkt_valid,read_enb_0,read_enb_1,read_enb_2,data_in,
				    busy,err,vld_out_0,vld_out_1,vld_out_2,data_out_0,data_out_1,data_out_2);
  
  input [7:0]data_in;
  input pkt_valid,clock,resetn,read_enb_0,read_enb_1,read_enb_2;
  output [7:0]data_out_0,data_out_1,data_out_2;
  output vld_out_0,vld_out_1,vld_out_2,err,busy;
	
	wire soft_reset_0,full_0,empty_0,soft_reset_1,full_1,empty_1,soft_reset_2,full_2,empty_2,
         fifo_full,detect_add,ld_state,laf_state,full_state,lfd_state,rst_int_reg,
         parity_done,low_packet_valid,write_enb_reg;
	wire [2:0]write_enb;
	wire [7:0]d_in;
	
    //-------fifo instantiation-----
    
    
	
	router_fifo FIFO_0(.clock(clock),
	                   .resetn(resetn),
		           .soft_reset(soft_reset_0),
			   .write_enb(write_enb[0]),
			   .read_enb(read_enb_0),
                           .lfd_state(lfd_state),
			   .data_in(d_in),
			   .full(full_0),
			   .empty(empty_0),
			   .data_out(data_out_0));
				   
				   
				   
				   
				   
	
	router_fifo FIFO_1(.clock(clock),
	                   .resetn(resetn),
			   .soft_reset(soft_reset_1),
			   .write_enb(write_enb[1]),
			   .read_enb(read_enb_1),
                           .lfd_state(lfd_state),
		           .data_in(d_in),
			   .full(full_1),
			   .empty(empty_1),
			   .data_out(data_out_1));
					   
					   
	
	router_fifo FIFO_2(.clock(clock),
	                   .resetn(resetn),
			   .soft_reset(soft_reset_2),
			   .write_enb(write_enb[2]),
			   .read_enb(read_enb_2),
                           .lfd_state(lfd_state),
			   .data_in(d_in),
			   .full(full_2),
			   .empty(empty_2),
			   .data_out(data_out_2));
    
    //-------register instantiation-----	
    
	router_reg REGISTER(.clock(clock),
	                    .resetn(resetn),
			    .pkt_valid(pkt_valid),
	        	    .data_in(data_in),
			    .fifo_full(fifo_full),
	        	    .detect_add(detect_add),
                            .ld_state(ld_state),
			    .laf_state(laf_state),
			    .full_state(full_state),
	       		    .lfd_state(lfd_state),
			    .rst_int_reg(rst_int_reg),
			    .err(err),
                            .parity_done(parity_done),
			    .low_packet_valid(low_packet_valid),
			    .dout(d_in));
				  
				  
				  
				  
    				
    //-------synchronizer instantiation-----

      
    
							 
	router_sync SYNCHRONIZER(.clock(clock),
	                         .resetn(resetn),
				 .data_in(data_in[1:0]),
				 .detect_add(detect_add),
				 .full_0(full_0),
				 .full_1(full_1),
				 .full_2(full_2),
				 .empty_0(empty_0),
				 .empty_1(empty_1),
				 .empty_2(empty_2),
				 .write_enb_reg(write_enb_reg),
				 .read_enb_0(read_enb_0),
				 .read_enb_1(read_enb_1),
				 .read_enb_2(read_enb_2),
				 .write_enb(write_enb),
				 .fifo_full(fifo_full),
				 .vld_out_0(vld_out_0),
				 .vld_out_1(vld_out_1),
				 .vld_out_2(vld_out_2),
				 .soft_reset_0(soft_reset_0),
				 .soft_reset_1(soft_reset_1),
				 .soft_reset_2(soft_reset_2));						 
							 
							 
							 
    //-------fsm instantiation-----
    
	router_fsm FSM(.clock(clock),
	               .resetn(resetn),
		       .pkt_valid(pkt_valid),
		       .data_in(data_in[1:0]),
		       .fifo_full(fifo_full),
		       .fifo_empty_0(empty_0),
		       .fifo_empty_1(empty_1),
		       .fifo_empty_2(empty_2),
                       .soft_reset_0(soft_reset_0),
		       .soft_reset_1(soft_reset_1),
		       .soft_reset_2(soft_reset_2),
		       .parity_done(parity_done),
		       .low_packet_valid(low_packet_valid),
                       .write_enb_reg(write_enb_reg),
	               .detect_add(detect_add),
		       .ld_state(ld_state),
		       .laf_state(laf_state),
		       .lfd_state(lfd_state),
		       .full_state(full_state),
	               .rst_int_reg(rst_int_reg),
	               .busy(busy));
   
endmodule


module router_sync(clock,resetn,data_in,detect_add,full_0,full_1,full_2,empty_0,empty_1,empty_2,write_enb_reg,read_enb_0,read_enb_1,read_enb_2,write_enb,fifo_full,vld_out_0,vld_out_1,vld_out_2,soft_reset_0,soft_reset_1,soft_reset_2);


input clock,resetn,detect_add,full_0,full_1,full_2,empty_0,empty_1,empty_2,write_enb_reg,read_enb_0,read_enb_1,read_enb_2;
input [1:0]data_in;
output reg[2:0]write_enb;
output reg fifo_full,soft_reset_0,soft_reset_1,soft_reset_2;
output vld_out_0,vld_out_1,vld_out_2;

  reg [1:0] data_in_tmp;
  reg[4:0]count0,count1,count2;
  
  always@(posedge clock)
  begin
    if(~resetn)
    data_in_tmp<=0;
    else if(detect_add)
    data_in_tmp<=data_in;
  end
  
  
  
//-----------Address decoding & fifo empty ---------------
always@(*)
  begin
    case(data_in_tmp)
    2'b00:begin
	  fifo_full<=full_0;
	  if(write_enb_reg)
	  write_enb<=3'b001;
	  else
	  write_enb<=0;
	  end
    2'b01:begin
	  fifo_full<=full_1;
	  if(write_enb_reg)
	  write_enb<=3'b010;
	  else
	  write_enb<=0;
	  end
    2'b10:begin
	  fifo_full<=full_2;
	  if(write_enb_reg)
	  write_enb<=3'b100;
	  else
	  write_enb<=0;
	  end
    default:begin
	  fifo_full<=0;
	  write_enb<=0;
	  end
    endcase
  end
  
  
  
//-----------------------------------Valid Byte block----------------------------------

assign vld_out_0 = (~empty_0);
assign vld_out_1 = (~empty_1);
assign vld_out_2 = (~empty_2);

  
//-----------------------------------Soft Reset block----------------------------------

always@(posedge clock)
  begin
  
  if(~resetn)
  begin
  count0<=0;
  soft_reset_0<=0;
  end

  else if(vld_out_0)
  begin
  if(~read_enb_0)
   
    begin
    if(count0==29)
      begin
      soft_reset_0<=1'b1;
      count0<=0;
      end
    else
      begin
      soft_reset_0<=1'b0;
      count0<=count0+1'b1;
      end
    end
  else
  count0<=0;
  end
  end

always@(posedge clock)
  begin
  
  if(~resetn)
  begin
  count1<=0;
  soft_reset_1<=0;
  end

  else if(vld_out_1)
  begin
  if(~read_enb_1)
   
    begin
    if(count1==29)
      begin
      soft_reset_1<=1'b1;
      count1<=0;
      end
    else
      begin
      soft_reset_1<=1'b0;
      count1<=count1+1'b1;
      end
    end
  else
  count1<=0;
  end
  end

always@(posedge clock)
  begin
  
  if(~resetn)
  begin
  count2<=0;
  soft_reset_2<=0;
  end

  else if(vld_out_2)
  begin
  if(~read_enb_2)
   
    begin
    if(count2==29)
      begin
      soft_reset_2<=1'b1;
      count2<=0;
      end
    else
      begin
      soft_reset_2<=1'b0;
      count2<=count2+1'b1;
      end
    end
  else
  count2<=0;
  end
  end

endmodule
  

module router_reg(clock,resetn,pkt_valid,data_in,fifo_full,detect_add,
                  ld_state,laf_state,full_state,lfd_state,rst_int_reg,err,
                  parity_done,low_packet_valid,dout);

input clock,resetn,pkt_valid,fifo_full,detect_add,ld_state,laf_state,full_state,lfd_state,rst_int_reg;
input [7:0]data_in;
output reg err,parity_done,low_packet_valid;
output reg [7:0]dout;
reg [7:0]header,int_reg,int_parity,ext_parity;
  
  
  //------------------------------DATA OUT LOGIC---------------------------------

	always@(posedge clock)
   	begin
      if(!resetn)
      	begin
	     dout    	 <=0;
	     header  	 <=0;
	     int_reg 	 <=0;
       	end
      else if(detect_add && pkt_valid && data_in[1:0]!=2'b11)
	     header<=data_in;
      else if(lfd_state)
	     dout<=header;
      else if(ld_state && !fifo_full)
	     dout<=data_in;
      else if(ld_state && fifo_full)
	     int_reg<=data_in;
      else if(laf_state)
	     dout<=int_reg;
     end

  //---------------------------LOW PACKET VALID LOGIC----------------------------
	
      	always@(posedge clock)
	   		begin
              if(!resetn)
	 				low_packet_valid<=0; 
         		else if(rst_int_reg)
	 				low_packet_valid<=0;

              else if(ld_state && !pkt_valid) 
         			low_packet_valid<=1;
			end
  //----------------------------PARITY DONE LOGIC--------------------------------
	
	always@(posedge clock)
	begin
      if(!resetn)
	  parity_done<=0;
     else if(detect_add)
	  parity_done<=0;
      else if((ld_state && !fifo_full && !pkt_valid)
              ||(laf_state && low_packet_valid && !parity_done))
	  parity_done<=1;
	end

//---------------------------PARITY CALCULATE LOGIC----------------------------

	always@(posedge clock)
	begin
      if(!resetn)
	 int_parity<=0;
	else if(detect_add)
	 int_parity<=0;
	else if(lfd_state && pkt_valid)
	 int_parity<=int_parity^header;
	else if(ld_state && pkt_valid && !full_state)
	 int_parity<=int_parity^data_in;
	else
	 int_parity<=int_parity;
	end
	 

//-------------------------------ERROR LOGIC-----------------------------------

	always@(posedge clock)
		begin
          if(!resetn)
	  			err<=0;
	      else if(parity_done)
	       		begin
	 				if (int_parity==ext_parity)
	    				err<=0;
	 				else 
	    			err<=1;
	 			end
	 	   else
	    		err<=0;
	      end

//-------------------------------EXTERNAL PARITY LOGIC-------------------------

	always@(posedge clock)
	begin
      if(!resetn)
	  		ext_parity<=0;
      else if(detect_add)
	  		ext_parity<=0;
      else if((ld_state && !fifo_full && !pkt_valid) || (laf_state && !parity_done && low_packet_valid))
	  		ext_parity<=data_in;
	 end

endmodule


module router_fsm(clock,resetn,pkt_valid,data_in,fifo_full,fifo_empty_0,fifo_empty_1,fifo_empty_2,
                  soft_reset_0,soft_reset_1,soft_reset_2,parity_done,low_packet_valid,
                  write_enb_reg,detect_add,ld_state,laf_state,lfd_state,full_state,rst_int_reg,busy);

input clock,resetn,pkt_valid,fifo_full,fifo_empty_0,fifo_empty_1,fifo_empty_2;
input soft_reset_0,soft_reset_1,soft_reset_2,parity_done,low_packet_valid;
input [1:0] data_in;
output write_enb_reg,detect_add,ld_state,laf_state,lfd_state,full_state,rst_int_reg,busy;
  
  
parameter DECODE_ADDRESS     = 3'b000,
          LOAD_FIRST_DATA 	 = 3'b001,
          LOAD_DATA 		 = 3'b010,
          WAIT_TILL_EMPTY 	 = 3'b011,
          CHECK_PARITY_ERROR = 3'b100,
          LOAD_PARITY 		 = 3'b101,
          FIFO_FULL_STATE 	 = 3'b110,
          LOAD_AFTER_FULL 	 = 3'b111;
  
  reg [2:0] PS,NS;
          
  always@(posedge clock)
    begin
      if(!resetn)
        PS <= DECODE_ADDRESS;
      else if(soft_reset_0 || soft_reset_1 || soft_reset_2)
        PS <= DECODE_ADDRESS;
      else 
        PS <= NS;
    end
  
  
  always@(*)
    begin
      NS = DECODE_ADDRESS;
      case(PS)
        
        DECODE_ADDRESS : 
                  begin
                    if((pkt_valid && (data_in[1:0]==0) && fifo_empty_0)||
                       (pkt_valid && (data_in[1:0]==1) && fifo_empty_1)||
                       (pkt_valid && (data_in[1:0]==2) && fifo_empty_2))
                      
                      NS = LOAD_FIRST_DATA;
                    
                    else if((pkt_valid && (data_in[1:0]==0) && (~fifo_empty_0))||
                            (pkt_valid && (data_in[1:0]==1) && (~fifo_empty_1))||
                            (pkt_valid && (data_in[1:0]==2) && (~fifo_empty_2)))
                      
                      NS = WAIT_TILL_EMPTY;
                    
                    else
                      NS = DECODE_ADDRESS;
                  end
        
        LOAD_FIRST_DATA :  NS = LOAD_DATA;
        
        LOAD_DATA       : 
                        begin
			      			if(fifo_full)
				 				NS=FIFO_FULL_STATE;
                          else if(!fifo_full && !pkt_valid)
				 				NS=LOAD_PARITY;
			      			else
				 				NS=LOAD_DATA;
			   			end 
        
        WAIT_TILL_EMPTY  : 
                        begin
                          if((!fifo_empty_0) || (!fifo_empty_1) || (!fifo_empty_2))
				 			NS=WAIT_TILL_EMPTY;
			      		  else if(fifo_empty_0||fifo_empty_1||fifo_empty_2)
				 			NS=LOAD_FIRST_DATA;
			      		  else
				 			NS=WAIT_TILL_EMPTY;
                         end
        
        CHECK_PARITY_ERROR:
          				begin
			    			if(fifo_full)
			      	 			NS=FIFO_FULL_STATE;
			    			else
			         			NS=DECODE_ADDRESS;

			   			 end

       LOAD_PARITY     	  :	NS=CHECK_PARITY_ERROR;
			 

       FIFO_FULL_STATE	  :	   
          				begin
                        	if(!fifo_full)
			         			NS=LOAD_AFTER_FULL;
			      			else if(fifo_full)
			         			NS=FIFO_FULL_STATE;
			  		 		end
        
        LOAD_AFTER_FULL:	   
          				begin
          					if((!parity_done) && (!low_packet_valid))
			   					NS=LOAD_DATA;
          					else if((!parity_done) && (low_packet_valid))
			   					NS=LOAD_PARITY;
          					else if(parity_done)
			   					NS=DECODE_ADDRESS;
			   			 end
        
      endcase
   end
  
  assign detect_add = ((PS==DECODE_ADDRESS)?1:0); 
  assign write_enb_reg=((PS==LOAD_DATA||PS==LOAD_PARITY||PS==LOAD_AFTER_FULL)?1:0);
  assign full_state=((PS==FIFO_FULL_STATE)?1:0);
  assign lfd_state=((PS==LOAD_FIRST_DATA)?1:0);
  assign busy=((PS==FIFO_FULL_STATE||PS==LOAD_AFTER_FULL||PS==WAIT_TILL_EMPTY||
       PS==LOAD_FIRST_DATA||PS==LOAD_PARITY||PS==CHECK_PARITY_ERROR)?1:0);
  //assign busy=((PS==LOAD_DATA || PS==DECODE_ADDRESS)?0:1);
  assign ld_state=((PS==LOAD_DATA)?1:0);
  assign laf_state=((PS==LOAD_AFTER_FULL)?1:0);
  assign rst_int_reg=((PS==CHECK_PARITY_ERROR)?1:0);
  
endmodule
  

module router_fifo(clock,resetn,soft_reset,write_enb,read_enb,
                   lfd_state,data_in,full,empty,data_out);
  
  input clock,resetn,soft_reset;
  input write_enb,read_enb,lfd_state;
  input [7:0]data_in;
  
  output reg [7:0] data_out;
  output full,empty;
  
  reg [4:0] rd_pointer,wr_pointer;
  reg [6:0] count;
  reg [8:0] mem [15:0];

  integer i;
  
   reg lfd_state_t;
  
  always@(posedge clock)
    begin
      if(!resetn)
        lfd_state_t <= 0;
      else
        lfd_state_t <= lfd_state;
    end 
   
  // ------Read Operation-------
  always@(posedge clock) 
    begin
      if(!resetn)
          data_out <= 8'b0;
      else if(soft_reset) 
          data_out <= 8'bz;
      else if((read_enb) && (!empty))
        data_out <= mem[rd_pointer[3:0]][7:0];
      else if(count == 0)
           data_out <= 8'bz;
    end
  
  // ------Write Operation------ 
  always@(posedge clock) 
    begin
      if(!resetn || soft_reset)
         begin
            for(i=0;i<16;i=i+1)
            mem[i]<=0;
         end
      else if(write_enb&&(~full))   
         begin
           if(lfd_state_t)
	           begin
                 mem[wr_pointer[3:0]][8]<=1'b1;
                 mem[wr_pointer[3:0]][7:0]<=data_in;
	           end
      
	      else
	           begin
                 mem[wr_pointer[3:0]][8]<=1'b0;
                 mem[wr_pointer[3:0]][7:0]<=data_in;
			   end
         end
     end

      
    
  
  //----Pointer generation block----

   always@(posedge clock) 
     begin
       if(!resetn)
        wr_pointer<=0;
      else if(write_enb && (~full))
        wr_pointer<=wr_pointer+1;
     end
   
   always@(posedge clock) //Read address
     begin
       if(!resetn)
         rd_pointer<=0;
       else if(read_enb && (~empty))
         rd_pointer<=rd_pointer+1;
     end
  
  
  //-----counter block while reading------
  
  always@(posedge clock)
    begin
      if(read_enb && !empty)
        begin
          if((mem[rd_pointer[3:0]][8])==1'b1)
            count <= mem[rd_pointer[3:0]][7:2] + 1'b1;
          else if(count != 0)
            count <= count - 1'b1;
        end
    end
  //---------Full & empty condition-----------
  assign full  = (wr_pointer ==({~rd_pointer[4],rd_pointer[3:0]}));
  assign empty = (rd_pointer == wr_pointer);
  
  
endmodule