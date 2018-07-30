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
-- ports:
--  reset ...... optional; when tied to '0' may yield <1word of poweron garbage
--  clock ...... synchronous clock (e.g. 200MHz in Zynq 7Z020)
--  en ......... enables capture, may be tied to '1'
--  inp ........ input signals to be analyzed (must be synchronized to clock!)
--  out_valid .. output FIFO strobe signal
--  out_data ... output FIFO data signal
--
-- parameters:
--  n_inp .. number of input signals
--  b_out .. number of bytes per one FIFO word (usually 4 or 8)
--
-- data format:
--  Produces a series of ProtoBuf VARINTs; longest VARINT is of the b_out
--  length. All VARINTs are tightly packed in out_data words, without spaces.
--  Each VARINT encodes: (d_time << n_inp) | inp, where d_time is time diff
--  between consecutive samples.
--
-- comments:
--  inp signals *must* be resynchronized to clock prior to leading into zlogan
--  Example (x is the non-synchronized input, x_sync leads to zlogan/inp):
--
--  constant n_resync: natural := 3;
--  type a_resync is array (natural range <>) of
--    std_logic_vector(n_resync-1 downto 0);
--  signal dfsync, nx_dfsync: a_resync (x'range);
--  signal x_sync: std_logic_vector(x'range);
--  --
--  g_sync: for k in x'range generate
--    nx_dfsync(k) <= x(k) & dfsync(k)(n_resync-1 downto 1);
--   x_sync(k) <= dfsync(k)(0);
--  end generate;
--  process
--  begin
--    wait until clock'event and clock = '1';
--    dfsync <= nx_dfsync;
--  end process;
------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity zlogan is
  generic (
    n_inp: natural := 4; -- number of input wires
    b_out: natural := 4 -- output FIFO width (4 or 8 [bytes])
  );
  port (
    reset: in std_logic;
    clock: in std_logic;
    en: in std_logic;
    inp: in std_logic_vector (n_inp-1 downto 0);
    out_valid: out std_logic;
    out_data: out std_logic_vector (b_out*8-1 downto 0)
  );
end zlogan;

-- TODO: flush last incomplete word after en-->'0'

architecture rtl of zlogan is
  constant w_time: natural := b_out*7 - n_inp;
  type a4slv8 is array (0 to b_out-1) of std_logic_vector(7 downto 0);
  type a3slv8 is array (0 to b_out-2) of std_logic_vector(7 downto 0);
  type a3a4slv8 is array (0 to b_out-1) of a4slv8;
  type a3a3slv8 is array (0 to b_out-1) of a3slv8;
  subtype bcnt_t is natural range 0 to b_out;
  type a3bcnt is array (0 to b_out-1) of bcnt_t;
  signal inp_word, nx_inp_word: a4slv8;
  signal buf, nx_buf: a3slv8;
  signal nx_buf_wait, nx_buf_ready: a3a3slv8;
  signal out_word: a3a4slv8;
  signal buf_len, nx_buf_len: natural range 0 to b_out-1;
  signal inp_len, nx_inp_len: bcnt_t;
  signal inp1, nx_inp1: std_logic_vector (b_out*7-1 downto 0);
  --
  signal inp1_zero: std_logic_vector (b_out-1 downto 0);
  signal inp1_full: std_logic_vector (b_out downto 0);
  signal inp1_bcnt: a3bcnt;
  --
  signal out0_data, nx_out0_data: std_logic_vector (out_data'range);
  signal event, out0_valid, nx_out0_valid, inp1_valid, nx_inp1_valid,
    time_hi: std_logic;
  signal time_cnt, nx_time_cnt: unsigned (w_time-1 downto 0);
  signal prev_inp: std_logic_vector (inp'range);
  signal prev_en: std_logic;
begin
  assert (b_out*7 - 3 > n_inp) report "Too many input ports!" severity failure;
  --
  -- i/o, event detection
  --
  out_valid <= out0_valid;
  out_data <= out0_data;
  time_hi <= '1' when time_cnt(time_cnt'high downto time_cnt'high-2) = "111"
             else '0';
  event <= '1' when ((en = '1') and ((inp /= prev_inp) or (time_hi = '1')))
                    or (en /= prev_en)
           else '0';
  nx_inp1 <= std_logic_vector(time_cnt) & inp;
  nx_inp1_valid <= event;

  --
  -- time diff
  --
  process (time_cnt, event)
  begin
    if event = '1' then
      nx_time_cnt <= (others => '0');
    else
      nx_time_cnt <= time_cnt + to_unsigned(1, time_cnt'length);
    end if;
  end process;

  --
  -- calculate number of utilized 7-bit tuples,
  -- pack into protobuf b_out*8-bit word
  --
  g1: for n in 0 to b_out-1 generate
    nx_inp_word(n) <= inp1_full(n+1) & inp1(7*n+6 downto 7*n);
    inp1_zero(n) <= '1' when inp1(7*n+6 downto 7*n) = b"000_0000"
                    else '0';
  end generate;
  inp1_full(b_out) <= '0';
  inp1_full(b_out-1) <= '0' when inp1_zero(b_out-1) = '1' else '1';
  g2: for n in 0 to b_out-2 generate
    inp1_full(n) <= inp1_full(n+1) or not inp1_zero(n);
  end generate;
  -- count number of utilized bytes
  inp1_bcnt(0) <= 1;  -- force min length of 1 byte even in case of all 0's
  g3: for n in 1 to b_out-1 generate
    inp1_bcnt(n) <= n+1 when inp1_full(n) = '1' else inp1_bcnt(n-1);
  end generate;
  nx_inp_len <= inp1_bcnt(b_out-1) when inp1_valid = '1'
                else 0;

  --
  -- assemble fixed b_out bytes long output word out of variable length input
  -- using b_out-1 length buffer
  -- state variable: number of currently held bytes in buf (buf_len)
  --
  g4: for bl in 0 to b_out-1 generate
    g41: for k in 0 to bl-1 generate
      nx_buf_ready(bl)(k) <= inp_word(b_out - bl + k);
      nx_buf_wait(bl)(k) <= buf(k);
      out_word(bl)(k) <= buf(k);
    end generate;
    g42: for k in bl to b_out-2 generate
      nx_buf_ready(bl)(k) <= (others => '-');
      nx_buf_wait(bl)(k) <= inp_word(k - bl);
      out_word(bl)(k) <= inp_word(k - bl);
    end generate;
    out_word(bl)(b_out-1) <= inp_word(b_out - 1 - bl);
  end generate;
  -- mux output word based on current buffer fill (buf_len)
  g5: for n in 0 to b_out-1 generate
    g51: for m in 0 to 7 generate
      nx_out0_data(n*8 + m) <= out_word(buf_len)(n)(m);
    end generate;
  end generate;
  
  process (buf_len, inp_len, nx_buf_ready, nx_buf_wait)
  begin
    -- mux nx_buf based on current buffer fill (buf_len)
    if buf_len + inp_len >= b_out then
      nx_out0_valid <= '1';
      nx_buf_len <= buf_len + inp_len - b_out;
      nx_buf <= nx_buf_ready(buf_len);
    else
      nx_out0_valid <= '0';
      nx_buf_len <= buf_len + inp_len;
      nx_buf <= nx_buf_wait(buf_len);
    end if;
  end process;

  --
  -- registers' update
  --
  process
  begin
    wait until rising_edge(clock);
    time_cnt <= nx_time_cnt;
    prev_inp <= inp;
    prev_en <= en;
    buf <= nx_buf;
    inp_len <= nx_inp_len;
    time_cnt <= nx_time_cnt;
    inp1 <= nx_inp1;
    inp1_valid <= nx_inp1_valid;
    inp_word <= nx_inp_word;
    out0_valid <= nx_out0_valid;
    out0_data <= nx_out0_data;
  end process;

  process (reset, clock)
  begin
    if reset = '1' then
      buf_len <= 0;
    elsif rising_edge(clock) then
      buf_len <= nx_buf_len;
    end if;
  end process;
end rtl;
