--
-- resynchronization of external asynchronous event
-- and its justification to span over exactly one target clok period
-- to use e.g. for CPU bus vs. external clock core signalling
-- ("data ready" etc.)
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity xevent is
  generic (
    n_resync: natural := 2  -- n of resync DFFs to reach given MTBF 
  );
  port (
    trig: in std_logic;
    clock: in std_logic;
    q: out std_logic
  );
end xevent;

architecture rtl of xevent is
  signal x: std_logic;
  signal y: std_logic_vector (n_resync+1 downto 0);
  attribute REGISTER_DUPLICATION : string;
  attribute REGISTER_DUPLICATION of y : signal is "NO";
begin
  y(y'high) <= x;
  q <= y(1) and not y(0);
  
  seq: process
  begin
    wait until clock'event and clock = '1';
    y(y'high-1 downto 0) <= y(y'high downto 1);
  end process;

  set: process (trig, clock)
  begin
    if trig = '1' then
      x <= '1';
    elsif clock'event and clock = '1' then
      x <= x and not y(1);
    end if;
  end process;
end rtl;

