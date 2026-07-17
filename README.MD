# GBA-HASHCAT

<img width="766" height="496" alt="Image" src="https://github.com/user-attachments/assets/06930cb4-8762-4cfa-8d38-bd41e377db7a" />

This is a "port" (not really) of the HashCat password cracker to the GAME BOY Advance, using the Butano engine.

To build, you'll probably have to modify the Makefile to point to where you have the Butano repo installed locally.

This uses the following 8MB wordlist: https://github.com/ignis-sec/Pwdb-Public/blob/master/wordlists/ignis-1M.txt 

It generates the SHA256 hash of each line, and compares it to the (hardcoded) target hash to see if we have a match.

<img width="777" height="557" alt="Image" src="https://github.com/user-attachments/assets/8c2cfe7e-52ec-4ede-b0a1-ec8c43d551c4" />

It is very slow and useless.
