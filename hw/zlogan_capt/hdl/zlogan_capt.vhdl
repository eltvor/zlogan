--
-- High-throughput ProtoBuf(RLE(delta_time | input)) Logic Analyzer
-- Copyright (c) 2017 Marek Peca <mp@eltvor.cz>
--
-- This source file is free software; you can redistribute it and/or
-- modify it under the terms of the GNU Lesser General Public
-- License as published by the Free Software Foundation; either
-- version 2.1 of the License, or (at your option) any later version.
--
-- This source file is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
-- Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public
-- License along with this library; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
--
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity zlogan_capt_v1_0 is
  generic (
    -- Users to add parameters here
    la_n_inp: natural := 4; -- number of input wires
    la_b_out: natural := 4; -- output FIFO width (4 or 8 [bytes])
    -- User parameters ends
    -- Do not modify the parameters beyond this line


    -- Parameters of Axi Slave Bus Interface S00_AXI
    C_S00_AXI_DATA_WIDTH  : integer  := 32;
    C_S00_AXI_ADDR_WIDTH  : integer  := 6;

    -- Parameters of Axi Master Bus Interface M00_AXIS
    C_M00_AXIS_TDATA_WIDTH  : integer  := 32;
    C_M00_AXIS_START_COUNT  : integer  := 32
  );
  port (
    -- Users to add ports here
    la_clock: in std_logic;
    la_inp: in std_logic_vector (la_n_inp-1 downto 0);

    fifo_data_count_i : in std_logic_vector(31 downto 0);
    fifo_wr_data_count_i : in std_logic_vector(31 downto 0);
    fifo_rd_data_count_i : in std_logic_vector(31 downto 0);
    
    fifo_reset_n : out std_logic;

    -- User ports ends
    -- Do not modify the ports beyond this line


    -- Ports of Axi Slave Bus Interface S00_AXI
    s00_axi_aclk  : in std_logic;
    s00_axi_aresetn  : in std_logic;
    s00_axi_awaddr  : in std_logic_vector(C_S00_AXI_ADDR_WIDTH-1 downto 0);
    s00_axi_awprot  : in std_logic_vector(2 downto 0);
    s00_axi_awvalid  : in std_logic;
    s00_axi_awready  : out std_logic;
    s00_axi_wdata  : in std_logic_vector(C_S00_AXI_DATA_WIDTH-1 downto 0);
    s00_axi_wstrb  : in std_logic_vector((C_S00_AXI_DATA_WIDTH/8)-1 downto 0);
    s00_axi_wvalid  : in std_logic;
    s00_axi_wready  : out std_logic;
    s00_axi_bresp  : out std_logic_vector(1 downto 0);
    s00_axi_bvalid  : out std_logic;
    s00_axi_bready  : in std_logic;
    s00_axi_araddr  : in std_logic_vector(C_S00_AXI_ADDR_WIDTH-1 downto 0);
    s00_axi_arprot  : in std_logic_vector(2 downto 0);
    s00_axi_arvalid  : in std_logic;
    s00_axi_arready  : out std_logic;
    s00_axi_rdata  : out std_logic_vector(C_S00_AXI_DATA_WIDTH-1 downto 0);
    s00_axi_rresp  : out std_logic_vector(1 downto 0);
    s00_axi_rvalid  : out std_logic;
    s00_axi_rready  : in std_logic;

    -- Ports of Axi Master Bus Interface M00_AXIS
    m00_axis_aclk  : in std_logic;
    m00_axis_aresetn  : in std_logic;
    m00_axis_tvalid  : out std_logic;
    m00_axis_tdata  : out std_logic_vector(C_M00_AXIS_TDATA_WIDTH-1 downto 0);
    m00_axis_tstrb  : out std_logic_vector((C_M00_AXIS_TDATA_WIDTH/8)-1 downto 0);
    m00_axis_tlast  : out std_logic;
    m00_axis_tready  : in std_logic
  );
end zlogan_capt_v1_0;

architecture arch_imp of zlogan_capt_v1_0 is

  -- component declaration
  component zlogan_capt_s00_axi is
    generic (
    la_n_inp : integer;
    S2MM_TDATA_WIDTH : integer;
    C_S_AXI_DATA_WIDTH  : integer  := 32;
    C_S_AXI_ADDR_WIDTH  : integer  := 6
    );
    port (
    reg_enable_o : out std_logic;
    reg_la_reset_o : out std_logic;
    reg_dma_trig_o : out std_logic;
    reg_dma_reset_o : out std_logic;
    reg_dma_len_o : out unsigned(29 downto 0);
    reg_dma_xrun_i : in std_logic;
    reg_state_mon_i : in std_logic_vector(2 downto 0);
    reg_count_mon_i : in unsigned (29 downto 0);
    fifo_data_count_i : in std_logic_vector(31 downto 0);
    fifo_wr_data_count_i : in std_logic_vector(31 downto 0);
    fifo_rd_data_count_i : in std_logic_vector(31 downto 0);
    fifo_reset_n_o : out std_logic;
    la_inp : in std_logic_vector(la_n_inp-1 downto 0);
    S_AXI_ACLK  : in std_logic;
    S_AXI_ARESETN  : in std_logic;
    S_AXI_AWADDR  : in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
    S_AXI_AWPROT  : in std_logic_vector(2 downto 0);
    S_AXI_AWVALID  : in std_logic;
    S_AXI_AWREADY  : out std_logic;
    S_AXI_WDATA  : in std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
    S_AXI_WSTRB  : in std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
    S_AXI_WVALID  : in std_logic;
    S_AXI_WREADY  : out std_logic;
    S_AXI_BRESP  : out std_logic_vector(1 downto 0);
    S_AXI_BVALID  : out std_logic;
    S_AXI_BREADY  : in std_logic;
    S_AXI_ARADDR  : in std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
    S_AXI_ARPROT  : in std_logic_vector(2 downto 0);
    S_AXI_ARVALID  : in std_logic;
    S_AXI_ARREADY  : out std_logic;
    S_AXI_RDATA  : out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
    S_AXI_RRESP  : out std_logic_vector(1 downto 0);
    S_AXI_RVALID  : out std_logic;
    S_AXI_RREADY  : in std_logic
    );
  end component zlogan_capt_s00_axi;

  component xevent is
    generic (
      n_resync: natural := 2  -- n of resync DFFs to reach given MTBF
      );
    port (
      trig: in std_logic;
      clock: in std_logic;
      q: out std_logic
      );
  end component;

  component zlogan_capt_rxdma is
    generic (
      S2MM_TDATA_WIDTH  : integer  := 32;
      S2MM_COUNT  : integer  := 32
    );
    port (
      aresetn, clock: in std_logic;
      data_in: in std_logic_vector (S2MM_TDATA_WIDTH-1 downto 0);
      time_in: in std_logic_vector (S2MM_TDATA_WIDTH-1 downto 0);
      valid: in std_logic;
      ready: out std_logic;
      xrun_flag: out std_logic;
      dma_trig, dma_reset: in std_logic;
      dma_len: in unsigned (29 downto 0);
      --
      state_mon_o : out std_logic_vector (2 downto 0);
      count_mon_o : out unsigned (29 downto 0);
      --
      S2MM_tvalid : out std_logic;
      S2MM_tready: in std_logic;
      S2MM_tdata: out std_logic_vector (S2MM_TDATA_WIDTH-1 downto 0);
      S2MM_tlast: out std_logic;
      S2MM_tstrb: out std_logic_vector(S2MM_TDATA_WIDTH/8-1 downto 0)
    );
  end component zlogan_capt_rxdma;

  component zlogan is
    generic (
      n_inp: natural := 4;
      b_out: natural := 4);
    port (
      reset: in std_logic;
      clock: in std_logic;
      en: in std_logic;
      inp: in std_logic_vector (n_inp-1 downto 0);
      out_valid: out std_logic;
      out_data: out std_logic_vector (b_out*8-1 downto 0));
  end component zlogan;

  signal dma_data, dma_tstamp: std_logic_vector (C_M00_AXIS_TDATA_WIDTH-1 downto 0);
  signal dma_data_valid, dma_trig, dma_rst, dma_xrun: std_logic;
  signal dma_len: unsigned (29 downto 0);
  signal count_mon: unsigned (29 downto 0);
  signal state_mon: std_logic_vector (2 downto 0);
  signal reg_enable : std_logic;
  signal la_reset : std_logic;
begin

-- Instantiation of Axi Bus Interface S00_AXI
zlogan_capt_s00_axi_inst : zlogan_capt_s00_axi
  generic map (
    la_n_inp => la_n_inp,
    S2MM_TDATA_WIDTH => C_M00_AXIS_TDATA_WIDTH,
    C_S_AXI_DATA_WIDTH  => C_S00_AXI_DATA_WIDTH,
    C_S_AXI_ADDR_WIDTH  => C_S00_AXI_ADDR_WIDTH
  )
  port map (
    S_AXI_ACLK  => s00_axi_aclk,
    S_AXI_ARESETN  => s00_axi_aresetn,
    S_AXI_AWADDR  => s00_axi_awaddr,
    S_AXI_AWPROT  => s00_axi_awprot,
    S_AXI_AWVALID  => s00_axi_awvalid,
    S_AXI_AWREADY  => s00_axi_awready,
    S_AXI_WDATA  => s00_axi_wdata,
    S_AXI_WSTRB  => s00_axi_wstrb,
    S_AXI_WVALID  => s00_axi_wvalid,
    S_AXI_WREADY  => s00_axi_wready,
    S_AXI_BRESP  => s00_axi_bresp,
    S_AXI_BVALID  => s00_axi_bvalid,
    S_AXI_BREADY  => s00_axi_bready,
    S_AXI_ARADDR  => s00_axi_araddr,
    S_AXI_ARPROT  => s00_axi_arprot,
    S_AXI_ARVALID  => s00_axi_arvalid,
    S_AXI_ARREADY  => s00_axi_arready,
    S_AXI_RDATA  => s00_axi_rdata,
    S_AXI_RRESP  => s00_axi_rresp,
    S_AXI_RVALID  => s00_axi_rvalid,
    S_AXI_RREADY  => s00_axi_rready,
    reg_enable_o => reg_enable,
    reg_dma_trig_o => dma_trig,
    reg_dma_reset_o => dma_rst,
    reg_dma_len_o => dma_len,
    reg_dma_xrun_i => dma_xrun,
    reg_la_reset_o => la_reset,
    reg_state_mon_i => state_mon,
    reg_count_mon_i => count_mon,
    fifo_data_count_i => fifo_data_count_i,
    fifo_wr_data_count_i => fifo_wr_data_count_i,
    fifo_rd_data_count_i => fifo_rd_data_count_i,
    fifo_reset_n_o => fifo_reset_n,
    la_inp => la_inp
  );

-- Instantiation of Axi Bus Interface M00_AXIS
  zlogan_capt_rxdma_inst : zlogan_capt_rxdma
  generic map (
    S2MM_TDATA_WIDTH  => C_M00_AXIS_TDATA_WIDTH,
    S2MM_COUNT  => C_M00_AXIS_START_COUNT
  )
  port map (
    aresetn => m00_axis_aresetn,
    clock => m00_axis_aclk,
    data_in => dma_data,
    time_in => dma_tstamp,
    valid => dma_data_valid,
    -- ready: out std_logic;
    xrun_flag => dma_xrun,
    dma_trig => dma_trig,
    dma_reset => dma_rst,
    dma_len => dma_len,
    state_mon_o => state_mon,
    count_mon_o => count_mon,
    S2MM_tvalid => m00_axis_tvalid,
    S2MM_tready => m00_axis_tready,
    S2MM_tdata => m00_axis_tdata,
    S2MM_tstrb => m00_axis_tstrb,
    S2MM_tlast => m00_axis_tlast
  );

  -- Add user logic here
zlogan0: zlogan
  generic map (n_inp => la_n_inp, b_out => la_b_out)
  port map (
    reset => la_reset,
    clock => la_clock,
    en => reg_enable,
    inp => la_inp,
    out_valid => dma_data_valid,
    out_data => dma_data
  );

  -- TODO: generate timestamp
  dma_tstamp <= (others => '0');
  
  -- User logic ends

end arch_imp;
