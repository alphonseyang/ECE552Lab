#include <stdio.h>

int main(int argc, char *argv[])
{
    // store the variables to registers, so that we will not load value from stack
    register int register1 = 0;
    register int register2 = 0;
    register int register3 = 0;

    // doest not sotre counter i in order to have load and store from stack opertaions instrution for variety
    int i;
    for (i = 0; i < 10000000; i++)
    {
        register1 = 1;
        register2 = 1;
        register3 = register1 + register2;
    }

    return 0;
}

/*

    219867 [xor: 0x5191518b] @ 0x004002b0: j         0x400248
    219868 [xor: 0x5191516b] @ 0x00400248: lw        r2,16(r30)  mem: 0x7fff7fd8
    219869 [xor: 0x5109137f] @ 0x00400250: lui       r6,0x98
    219870 [xor: 0x51098530] @ 0x00400258: ori       r6,r6,38527
                                                                        //RAW hazard, lui writes r6 and then ori reads r6, two cycles stall
    219871 [xor: 0x5109c704] @ 0x00400260: slt       r2,r6,r2           //Compare the counter i
                                                                        //RAW hazard, ori writes r6 and then slt reads r6, two cycles stall
    219872 [xor: 0x5109c71c] @ 0x00400268: beq       r2,r0,0x400278     //Compare the counter i
                                                                        //RAW hazard, slt writes r2 and then beq reads r2, two cycles stall
    219873 [xor: 0x5109c7f4] @ 0x00400278: addiu     r3,r0,1            //Assign value 1 to variable register1
    219874 [xor: 0x5109c704] @ 0x00400280: addiu     r4,r0,1            //Assign value 1 to variable register2
    219875 [xor: 0x5109c714] @ 0x00400288: addu      r5,r3,r4           //Sum up registers
                                                                        //RAW hazard, addiu writes r4 and then addu reads r4, two cycles stall
                                                                        //hidden RAW hazard, since we can only have one hazard per instruction, so the one cycle stall for the r3 RAW hazard is hidden by 2 cycles stall previously
    219876 [xor: 0x5191137f] @ 0x00400290: lw        r6,16(r30)  mem: 0x7fff7fd8
    219877 [xor: 0x5191514a] @ 0x00400298: addiu     r2,r6,1            //Increment counter i
                                                                        //RAW hazard, lw writes r6 and then addiu reads r6, two cycles stall
    219878 [xor: 0x5191517b] @ 0x004002a0: addu      r6,r0,r2
                                                                        //RAW hazard, addiu writes r2 and then addu reads r2, two cycles stall
    219879 [xor: 0x5191516b] @ 0x004002a8: sw        r6,16(r30)  mem: 0x7fff7fd8
                                                                        //RAW hazard, addu writes r6 and then sw reads r6, two cycles stall
    219880 [xor: 0x5191518b] @ 0x004002b0: j         0x400248

There are about 13 instructions per one loop, and the possible RAW hazards are 7 (7 two cycles stall), so 13+7*2 = 27 cycles, around 27/13 = 2.077CPI 
*/
