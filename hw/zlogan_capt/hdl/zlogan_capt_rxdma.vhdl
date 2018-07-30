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

entity zlogan_capt_rxdma is
  generic (
    -- Users to add parameters here

    -- User parameters ends
    -- Do not modify the parameters beyond this line

    -- Width of S_AXIS address bus. The slave accepts the read and write addresses of width C_M_AXIS_TDATA_WIDTH.
    S2MM_TDATA_WIDTH  : integer  := 32;
    -- Start count is the number of clock cycles the master will wait before initiating/issuing any transaction.
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
end zlogan_capt_rxdma;

architecture rtl of zlogan_capt_rxdma is
  type dma_state_t is (ST_WAIT_TRIG, ST_STAMP, ST_DATA);
  signal state, nx_state: dma_state_t;
  signal count, nx_count: unsigned (29 downto 0);
  signal count_lo: unsigned (19 downto 0);
  signal count_hi: unsigned (9 downto 0);
  constant cnt_zeros: unsigned (count'range) := (others => '0');
  constant cnt_ones: unsigned (count'range) := (others => '1');
  constant cnt_lo_zeros: unsigned (count_lo'range) := (others => '0');
  constant cnt_hi_zeros: unsigned (count_hi'range) := (others => '0');
  signal ready_buf, xrun_flag_rg, dma_valid,
    nx_xrun_flag, nx_dma_valid: std_logic;
  signal dma_data, nx_dma_data: std_logic_vector (S2MM_TDATA_WIDTH-1 downto 0);
  signal nx_state_mon : std_logic_vector (2 downto 0);
begin
  ready <= ready_buf;
  xrun_flag <= xrun_flag_rg;
  count_lo <= count(19 downto 0);
  count_hi <= count(29 downto 20);
  
  buf: process (dma_data, dma_valid, dma_reset, xrun_flag_rg,
                state, ready_buf, data_in, S2MM_tready)
  begin
    nx_dma_data <= dma_data;
    nx_dma_valid <= dma_valid;
    nx_xrun_flag <= xrun_flag_rg and not dma_reset;
    if state = ST_DATA then
      ready_buf <= not dma_valid;
    else
      ready_buf <= '0';
    end if;
    if valid = '1' then
      nx_dma_data <= data_in;
      nx_dma_valid <= '1';
      if (ready_buf or S2MM_tready) = '0' then
        nx_xrun_flag <= '1';
      end if;
    else
      if (dma_valid = '1') and
        (state = ST_DATA) and
        (S2MM_tready = '1') then
        nx_dma_valid <= '0';
      end if;
    end if;
  end process;

  fsm: process (state, count, count_lo, count_hi, dma_trig, dma_data, dma_len,
                dma_valid, S2MM_tready)
  begin
    S2MM_tvalid <= '0';
    S2MM_tlast <= '0';
    nx_state <= state;
    nx_count <= count;
    S2MM_tdata <= dma_data;
    S2MM_tstrb <= (others => '1');
    count_mon_o <= count;
    --
    case state is
      when ST_WAIT_TRIG =>
        nx_state_mon <= "001";
        if dma_trig = '1' then
          nx_state <= ST_STAMP;
          nx_count <= dma_len;
        end if;

      when ST_STAMP =>
        nx_state_mon <= "010";
        S2MM_tvalid <= '1';
        S2MM_tdata <= time_in;
        if count_lo = cnt_lo_zeros then
          S2MM_tlast <= '1';
          if S2MM_tready = '1' then
            nx_state <= ST_WAIT_TRIG;
          end if;
        else
          if S2MM_tready = '1' then
            nx_count <= count + cnt_ones;  -- count = count - 1
            nx_state <= ST_DATA;
          end if;
        end if;

      when ST_DATA =>
        nx_state_mon <= "100";
        if dma_valid = '1' then
          S2MM_tvalid <= '1';
          if count_lo = cnt_lo_zeros then
            S2MM_tlast <= '1';
            if S2MM_tready = '1' then
              nx_count <= count + cnt_ones;  -- count = count - 1
              if count_hi = cnt_hi_zeros then
                nx_state <= ST_WAIT_TRIG;
              else
                nx_state <= ST_STAMP;
              end if;
            end if;
          else
            if S2MM_tready = '1' then
              nx_count <= count + cnt_ones;  -- count = count - 1
            end if;
          end if;
        else
          S2MM_tvalid <= '0';
        end if;

      when others =>
    -- nihil
    end case;
  end process;

  process
  begin
    wait until rising_edge(clock);
    count <= nx_count;
    dma_data <= nx_dma_data;
    dma_valid <= nx_dma_valid;
    xrun_flag_rg <= nx_xrun_flag;
    state_mon_o <= nx_state_mon;
  end process;

  process (aresetn, clock)
  begin
    if aresetn = '0' or dma_reset = '1' then
      state <= ST_WAIT_TRIG;
    elsif rising_edge(clock) then
      state <= nx_state;
    end if;
  end process;
end rtl;
