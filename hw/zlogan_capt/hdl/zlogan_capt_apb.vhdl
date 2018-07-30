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

entity zlogan_capt_apb is
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
end entity;

architecture rtl of zlogan_capt_apb is
    signal slv_reg0  : std_logic_vector(31 downto 0);
    signal slv_reg1  : std_logic_vector(31 downto 0);
    signal slv_reg2  : std_logic_vector(31 downto 0);
    signal slv_reg3  : std_logic_vector(31 downto 0);
    signal slv_reg4  : std_logic_vector(31 downto 0);
    signal slv_reg5  : std_logic_vector(31 downto 0);
    signal slv_reg6  : std_logic_vector(31 downto 0);
    signal slv_reg7  : std_logic_vector(31 downto 0);
    signal slv_reg8  : std_logic_vector(31 downto 0);

    signal reg_addr  : std_logic_vector(7 downto 0);

    function apply_be(constant reg      : in  std_logic_vector(31 downto 0);
                      constant wrsignal : in  std_logic_vector(31 downto 0);
                      constant be       : in  std_logic_vector(3 downto 0))
                      return std_logic_vector is
        variable res : std_logic_vector(31 downto 0);
    begin
        res := reg;
        for i in be'range loop
            if be(i) = '1' then
                res((i+1)*8 downto i*8) := wrsignal((i+1)*8 downto i*8);
            end if;
        end loop;
        return res;
    end function apply_be;
begin
    -- aligned
    reg_addr <= s_apb_paddr(reg_addr'left+2 downto 2);

    p:process(aclk, arstn)
    begin
        if arstn = '0' then
            slv_reg0 <= (others => '0');
            --slv_reg1 <= (others => '0');
            slv_reg2 <= (others => '0');
            --slv_reg3 <= (others => '0');
            --slv_reg4 <= (others => '0');
            --slv_reg5 <= (others => '0');
            --slv_reg6 <= (others => '0');
            --slv_reg7 <= (others => '0');
            --slv_reg8 <= (others => '0');
        elsif rising_edge(aclk) then
            if s_apb_psel = '1' and s_apb_penable = '1' then
                if s_apb_pwrite = '1' then
                    case reg_addr is
                        when x"00" => slv_reg0 <= apply_be(slv_reg0, s_apb_pwdata, s_apb_pstrb);
                        --when x"01" => slv_reg1 <= apply_be(slv_reg1, s_apb_pwdata, s_apb_pstrb);
                        when x"02" => slv_reg2 <= apply_be(slv_reg2, s_apb_pwdata, s_apb_pstrb);
                        --when x"03" => slv_reg3 <= apply_be(slv_reg3, s_apb_pwdata, s_apb_pstrb);
                        --when x"04" => slv_reg4 <= apply_be(slv_reg4, s_apb_pwdata, s_apb_pstrb);
                        --when x"05" => slv_reg5 <= apply_be(slv_reg5, s_apb_pwdata, s_apb_pstrb);
                        --when x"06" => slv_reg6 <= apply_be(slv_reg6, s_apb_pwdata, s_apb_pstrb);
                        --when x"07" => slv_reg7 <= apply_be(slv_reg7, s_apb_pwdata, s_apb_pstrb);
                        --when x"08" => slv_reg8 <= apply_be(slv_reg8, s_apb_pwdata, s_apb_pstrb);
                        --others =>  --set error?
                    end case;
                else
                    case reg_addr is
                        when x"00" => s_apb_prdata <= slv_reg0;
                        when x"01" => s_apb_prdata <= slv_reg1;
                        when x"02" => s_apb_prdata <= slv_reg2;
                        when x"03" => s_apb_prdata <= slv_reg3;
                        when x"04" => s_apb_prdata <= slv_reg4;
                        when x"05" => s_apb_prdata <= slv_reg5;
                        when x"06" => s_apb_prdata <= slv_reg6;
                        when x"07" => s_apb_prdata <= slv_reg7;
                        when x"08" => s_apb_prdata <= slv_reg8;
                        when others=> s_apb_prdata <= (others => '0'); --set error?
                    end case;
                end if;
            end if;
        end if;
    end process;

    -- 0: CR
    reg_la_reset_o <= slv_reg0(0) or not arstn;
    fifo_reset_n_o <= not slv_reg0(1) and arstn;
    reg_dma_reset_o <= slv_reg0(2) or not arstn;
    reg_enable_o <= slv_reg0(8);
    reg_dma_trig_o <= slv_reg0(9);

    -- 1: SR
    slv_reg1(0) <= reg_dma_xrun_i;
    slv_reg1(15 downto 1) <= (others => '0');
    slv_reg1(18 downto 16) <= reg_state_mon_i;
    slv_reg1(slv_reg1'left downto 19) <= (others => '0');

    -- 2: LEN
    reg_dma_len_o <= unsigned(slv_reg2(29 downto 0));

    -- 3: INP
    slv_reg3(la_n_inp-1 downto 0) <= la_inp;
    slv_reg3(slv_reg3'left downto la_n_inp) <= (others => '0');

    -- 4: ID
    slv_reg4(7 downto 0) <= x"01"; -- IP version
    slv_reg4(15 downto 8) <= std_logic_vector(to_unsigned(S2MM_TDATA_WIDTH/8, 8)); -- word size
    slv_reg4(23 downto 16) <= std_logic_vector(to_unsigned(la_n_inp, 8)); -- nsignals
    slv_reg4(slv_reg4'left downto 24) <= (others => '0');

    --
    slv_reg5 <= fifo_data_count_i;
    slv_reg6 <= fifo_rd_data_count_i;
    slv_reg7 <= fifo_wr_data_count_i;

    -- 8: SHADOW_LEN
    slv_reg8(29 downto 0) <= std_logic_vector(reg_count_mon_i);
    slv_reg8(slv_reg8'left downto 30) <= (others => '0');
end architecture rtl;
