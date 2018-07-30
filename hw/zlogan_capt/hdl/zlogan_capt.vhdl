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

    -- Parameters of Axi Master Bus Interface M00_AXIS
    C_M00_AXIS_TDATA_WIDTH  : integer  := 32;
    C_M00_AXIS_START_COUNT  : integer  := 32
  );
  port (
    --la_clock: in std_logic; <= aclk
    la_inp: in std_logic_vector (la_n_inp-1 downto 0);

    fifo_data_count_i : in std_logic_vector(31 downto 0);
    fifo_wr_data_count_i : in std_logic_vector(31 downto 0);
    fifo_rd_data_count_i : in std_logic_vector(31 downto 0);

    fifo_reset_n : out std_logic;

    timestamp            : in  std_logic_vector(C_M00_AXIS_TDATA_WIDTH-1 downto 0);

    -- Ports of APB Interface
    aclk                 : in  std_logic;
    arstn                : in  std_logic;
    s_apb_paddr          : in  std_logic_vector(31 downto 0);
    s_apb_penable        : in  std_logic;
    s_apb_pprot          : in  std_logic_vector(2 downto 0);
    s_apb_prdata         : out std_logic_vector(31 downto 0);
    s_apb_pready         : out std_logic;
    s_apb_psel           : in  std_logic;
    s_apb_pslverr        : out std_logic;
    s_apb_pstrb          : in  std_logic_vector(3 downto 0);
    s_apb_pwdata         : in  std_logic_vector(31 downto 0);
    s_apb_pwrite         : in  std_logic;

    -- Ports of Axi Master Bus Interface M00_AXIS
    m00_axis_aclk        : in std_logic;
    m00_axis_aresetn     : in std_logic;
    m00_axis_tvalid      : out std_logic;
    m00_axis_tdata       : out std_logic_vector(C_M00_AXIS_TDATA_WIDTH-1 downto 0);
    m00_axis_tstrb       : out std_logic_vector((C_M00_AXIS_TDATA_WIDTH/8)-1 downto 0);
    m00_axis_tlast       : out std_logic;
    m00_axis_tready      : in std_logic
  );
end zlogan_capt_v1_0;

architecture arch_imp of zlogan_capt_v1_0 is
  -- component declaration
  component zlogan_capt_apb is
    generic (
      la_n_inp : integer;
      S2MM_TDATA_WIDTH : integer
    );
    port (
      aclk                 : in  std_logic;
      arstn                : in  std_logic;

      reg_enable_o         : out std_logic;
      reg_dma_trig_o       : out std_logic;
      reg_dma_reset_o      : out std_logic;
      reg_dma_len_o        : out unsigned(29 downto 0);
      reg_dma_xrun_i       : in  std_logic;
      reg_state_mon_i      : in  std_logic_vector(2 downto 0);
      reg_count_mon_i      : in  unsigned(29 downto 0);
      reg_la_reset_o       : out std_logic;
      fifo_data_count_i    : in  std_logic_vector(31 downto 0);
      fifo_wr_data_count_i : in  std_logic_vector(31 downto 0);
      fifo_rd_data_count_i : in  std_logic_vector(31 downto 0);
      fifo_reset_n_o       : out std_logic;
      la_inp               : in  std_logic_vector(la_n_inp-1 downto 0);

      s_apb_paddr          : in  std_logic_vector(31 downto 0);
      s_apb_penable        : in  std_logic;
      s_apb_pprot          : in  std_logic_vector(2 downto 0);
      s_apb_prdata         : out std_logic_vector(31 downto 0);
      s_apb_pready         : out std_logic;
      s_apb_psel           : in  std_logic;
      s_apb_pslverr        : out std_logic;
      s_apb_pstrb          : in  std_logic_vector(3 downto 0);
      s_apb_pwdata         : in  std_logic_vector(31 downto 0);
      s_apb_pwrite         : in  std_logic
    );
  end component;

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
  signal la_clock : std_logic;
begin
  la_clock <= aclk;

  -- Instantiation of APB Interface
  zlogan_apb_capt_inst: zlogan_capt_apb
  generic map (
    la_n_inp             => la_n_inp,
    S2MM_TDATA_WIDTH     => C_M00_AXIS_TDATA_WIDTH
  )
  port map (
    aclk                 => aclk,
    arstn                => arstn,

    reg_enable_o         => reg_enable,
    reg_dma_trig_o       => dma_trig,
    reg_dma_reset_o      => dma_rst,
    reg_dma_len_o        => dma_len,
    reg_dma_xrun_i       => dma_xrun,
    reg_state_mon_i      => state_mon,
    reg_count_mon_i      => count_mon,
    reg_la_reset_o       => la_reset,
    fifo_data_count_i    => fifo_data_count_i,
    fifo_wr_data_count_i => fifo_wr_data_count_i,
    fifo_rd_data_count_i => fifo_rd_data_count_i,
    fifo_reset_n_o       => fifo_reset_n,
    la_inp               => la_inp,

    s_apb_paddr          => s_apb_paddr,
    s_apb_penable        => s_apb_penable,
    s_apb_pprot          => s_apb_pprot,
    s_apb_prdata         => s_apb_prdata,
    s_apb_pready         => s_apb_pready,
    s_apb_psel           => s_apb_psel,
    s_apb_pslverr        => s_apb_pslverr,
    s_apb_pstrb          => s_apb_pstrb,
    s_apb_pwdata         => s_apb_pwdata,
    s_apb_pwrite         => s_apb_pwrite
  );

-- Instantiation of Axi Bus Interface M00_AXIS
  zlogan_capt_rxdma_inst : zlogan_capt_rxdma
  generic map (
    S2MM_TDATA_WIDTH => C_M00_AXIS_TDATA_WIDTH,
    S2MM_COUNT       => C_M00_AXIS_START_COUNT
  )
  port map (
    aresetn     => m00_axis_aresetn,
    clock       => m00_axis_aclk,
    data_in     => dma_data,
    time_in     => dma_tstamp,
    valid       => dma_data_valid,
    -- ready: out std_logic;
    xrun_flag   => dma_xrun,
    dma_trig    => dma_trig,
    dma_reset   => dma_rst,
    dma_len     => dma_len,
    state_mon_o => state_mon,
    count_mon_o => count_mon,
    S2MM_tvalid => m00_axis_tvalid,
    S2MM_tready => m00_axis_tready,
    S2MM_tdata  => m00_axis_tdata,
    S2MM_tstrb  => m00_axis_tstrb,
    S2MM_tlast  => m00_axis_tlast
  );

  -- Add user logic here
zlogan0: zlogan
  generic map (
    n_inp       => la_n_inp,
    b_out       => la_b_out
  )
  port map (
    reset       => la_reset,
    clock       => la_clock,
    en          => reg_enable,
    inp         => la_inp,
    out_valid   => dma_data_valid,
    out_data    => dma_data
  );

  dma_tstamp <= timestamp;

  -- User logic ends

end arch_imp;
