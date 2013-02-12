This is a fork of the Open-Beacon proximity tag code building on the work of Travis Goodspeed. 


The following pin configuration was used using an ez430-rf2500t and a nRF24l01+. The IRQ pin is not used. Pin configurations can be changed in config.h

       -----------                      -------------
       |         | -- P3.1 ---> MOSI -- |           |
       |         | -- P3.2 ---> MISO -- |           |
       |         | -- P3.3 ---> SCK  -- |           |
       |         | -- P2.0 ---> CSN  -- |           |
       | MSP430  | -- P2.1 ---> CE   -- | nRF24l01+ |
       |         |           NC IRQ  -- |           |
       |         |            $ GND  -- |           |
       |         |            $ VS   -- |           |
       -----------                      -------------

msp430-gcc (open-source) is used to build the program.

You can flash the ez430-rf2500 using mspdebug:
 $ mspdebug rf2500 "prog openbeacontag.hex"
