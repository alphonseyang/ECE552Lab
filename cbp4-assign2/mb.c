#include <stdio.h>

int main()
{
    // store the variables to registers, so that we will not load value from stack
    register int a = 0;
    register int b = 0;
    register int i;
    for (i = 0; i < 10000; i++)
    {
        // Pattern: TNNNNNTNNNNNTNNNNNT
        // a 5-bit can cover the pattern
        // our BHT entries is 6-bit long, can catch all the patterns above
        if ((a % 6) == 0)
            a = 0;
        
        // Pattern TNNNNNNNTNNNNNNNTNNNNNNNT
        // this pattern needs at least 7 bits BHR
        // so our BHT entries may not be able to catch all patterns
        // prediction may not too accurate
        if ((b % 8) == 0)
            b = 8;
            
        a++;
        b++;
     }
     
     return 0;
}

/*
0000000000000660 <main>:
 660:	55                   	push   %rbp
 661:	48 89 e5             	mov    %rsp,%rbp
 664:	41 55                	push   %r13
 666:	41 54                	push   %r12
 668:	53                   	push   %rbx
 669:	bb 00 00 00 00       	mov    $0x0,%ebx
 66e:	41 bc 00 00 00 00    	mov    $0x0,%r12d
 674:	41 bd 00 00 00 00    	mov    $0x0,%r13d
 67a:	eb 40                	jmp    6bc <main+0x5c>        // loop jump
 67c:	ba ab aa aa 2a       	mov    $0x2aaaaaab,%edx
 681:	89 d8                	mov    %ebx,%eax
 683:	f7 ea                	imul   %edx
 685:	89 d8                	mov    %ebx,%eax
 687:	c1 f8 1f             	sar    $0x1f,%eax
 68a:	29 c2                	sub    %eax,%edx
 68c:	89 d0                	mov    %edx,%eax
 68e:	01 c0                	add    %eax,%eax
 690:	01 d0                	add    %edx,%eax
 692:	01 c0                	add    %eax,%eax
 694:	89 da                	mov    %ebx,%edx
 696:	29 c2                	sub    %eax,%edx
 698:	85 d2                	test   %edx,%edx
 69a:	75 05                	jne    6a1 <main+0x41>        // check the multiple of 6, this shoule be covered by our private BHT entires, since it can be covered with 6-bit BHT entries
 69c:	bb 00 00 00 00       	mov    $0x0,%ebx
 6a1:	44 89 e0             	mov    %r12d,%eax
 6a4:	83 e0 07             	and    $0x7,%eax
 6a7:	85 c0                	test   %eax,%eax
 6a9:	75 06                	jne    6b1 <main+0x51>        // check the mulitple of 8, this may not be covered by our private BHT entires, the length of the pattern exceeds 6-bit
 6ab:	41 bc 08 00 00 00    	mov    $0x8,%r12d
 6b1:	83 c3 01             	add    $0x1,%ebx
 6b4:	41 83 c4 01          	add    $0x1,%r12d
 6b8:	41 83 c5 01          	add    $0x1,%r13d
 6bc:	41 81 fd 0f 27 00 00 	cmp    $0x270f,%r13d
 6c3:	7e b7                	jle    67c <main+0x1c>        // loop check
 6c5:	b8 00 00 00 00       	mov    $0x0,%eax
 6ca:	5b                   	pop    %rbx
 6cb:	41 5c                	pop    %r12
 6cd:	41 5d                	pop    %r13
 6cf:	5d                   	pop    %rbp
 6d0:	c3                   	retq   
 6d1:	66 2e 0f 1f 84 00 00 	nopw   %cs:0x0(%rax,%rax,1)
 6d8:	00 00 00 
 6db:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)

*/
