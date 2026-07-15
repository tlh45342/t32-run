#!/usr/bin/env python3
from pathlib import Path
import struct, subprocess, tempfile

ROOT=Path(__file__).resolve().parents[1]
RUN=ROOT/'bin'/'t32-run'
BASE=0x1000

def ins(op,rd=0,ra=0,rb=0): return (op<<24)|(rd<<20)|(ra<<16)|(rb<<12)
def words(*xs): return b''.join(struct.pack('<I',x&0xffffffff) for x in xs)

def run_case(name, image, steps, checks):
    with tempfile.TemporaryDirectory() as td:
        td=Path(td); (td/'test.bin').write_bytes(image)
        script=f'''load test.bin 0x1000\nset pc 0x1000\nset run steps {steps}\nrun\nregs\nstatus\n'''
        p=subprocess.run([str(RUN)],cwd=td,input=script,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        ok=p.returncode==0 and all(c in p.stdout for c in checks)
        print(('PASS' if ok else 'FAIL'),name)
        if not ok:
            print(p.stdout)
            print('missing:',[c for c in checks if c not in p.stdout])
        return ok

# Canonical opcodes
H,N,T,IR,CPU=0,1,2,3,4
MOV,MOVI=8,9
LDB,LDH,LDW,STB,STH,STW=16,17,18,19,20,21
ADD,ADDI,SUB,SUBI,MUL,MULU,DIV,DIVU=24,25,26,27,28,29,30,31
AND,OR,XOR,NOT,SHL,SHR,SAR=32,33,34,35,36,37,38
CMP,CMPI,JMP,JZ,JNZ=40,41,42,43,44
PUSH,POP,CALL,RET=48,49,50,51

cases=[]
def add(name,img,steps,*checks): cases.append((name,img,steps,checks))
add('HALT',words(ins(H)),1,'state=halted','instructions=1')
add('NOP',words(ins(N),ins(H)),2,'pc =0x00001008','instructions=2')
add('TRAP',words(ins(T),7),1,'state=halted','reason=TRAP 0x00000007')
# IRET: push target, iret, target halt
add('IRET',words(ins(MOVI,15),0x3000,ins(MOVI,0),BASE+24,ins(PUSH,0,0),ins(IR),ins(H)),5,'pc =0x0000101c','state=halted')
add('CPUID',words(ins(CPU,0),ins(H)),2,'r0 =0x54333201')
add('MOV',words(ins(MOVI,0),42,ins(MOV,1,0),ins(H)),3,'r1 =0x0000002a')
add('MOVI',words(ins(MOVI,0),42,ins(H)),2,'r0 =0x0000002a')
# memory family using address 0x2000
for name,st,ld,val,expect in [('LDB/STB',STB,LDB,0x1234562a,'r2 =0x0000002a'),('LDH/STH',STH,LDH,0x12342a2b,'r2 =0x00002a2b'),('LDW/STW',STW,LDW,0x12342a2b,'r2 =0x12342a2b')]:
    add(name,words(ins(MOVI,0),0x2000,ins(MOVI,1),val,ins(st,0,0,1),ins(ld,2,0),ins(H)),5,expect,'state=halted')
# arithmetic
for name,op,a,b,expect in [('ADD',ADD,20,22,42),('SUB',SUB,42,20,22),('MUL',MUL,6,7,42),('MULU',MULU,6,7,42),('DIV',DIV,84,2,42),('DIVU',DIVU,84,2,42),('AND',AND,0x2f,0x2a,0x2a),('OR',OR,0x20,0x0a,0x2a),('XOR',XOR,0x20,0x0a,0x2a)]:
    add(name,words(ins(MOVI,0),a,ins(MOVI,1),b,ins(op,2,0,1),ins(H)),4,f'r2 =0x{expect:08x}')
for name,op,a,imm,expect in [('ADDI',ADDI,20,22,42),('SUBI',SUBI,42,20,22)]:
    add(name,words(ins(MOVI,0),a,ins(op,1,0),imm,ins(H)),3,f'r1 =0x{expect:08x}')
add('NOT',words(ins(MOVI,0),0xffffffd5,ins(NOT,1,0),ins(H)),3,'r1 =0x0000002a')
add('SHL',words(ins(MOVI,0),21,ins(MOVI,1),1,ins(SHL,2,0,1),ins(H)),4,'r2 =0x0000002a')
add('SHR',words(ins(MOVI,0),84,ins(MOVI,1),1,ins(SHR,2,0,1),ins(H)),4,'r2 =0x0000002a')
add('SAR',words(ins(MOVI,0),0xffffffac,ins(MOVI,1),1,ins(SAR,2,0,1),ins(H)),4,'r2 =0xffffffd6','negative=1')
add('CMP',words(ins(MOVI,0),42,ins(MOVI,1),42,ins(CMP,0,0,1),ins(H)),4,'zero=1')
add('CMPI',words(ins(MOVI,0),42,ins(CMPI,0,0),42,ins(H)),3,'zero=1')
# branches
add('JMP',words(ins(JMP),BASE+16,ins(MOVI,0),99,ins(H)),2,'r0 =0x00000000','state=halted')
add('JZ',words(ins(MOVI,0),0,ins(JZ,0,0),BASE+24,ins(MOVI,1),99,ins(MOVI,1),42,ins(H)),4,'r1 =0x0000002a')
add('JNZ',words(ins(MOVI,0),1,ins(JNZ,0,0),BASE+24,ins(MOVI,1),99,ins(MOVI,1),42,ins(H)),4,'r1 =0x0000002a')
# stack
add('PUSH',words(ins(MOVI,15),0x3000,ins(MOVI,0),42,ins(PUSH,0,0),ins(H)),4,'r15=0x00002ffc')
add('POP',words(ins(MOVI,15),0x3000,ins(MOVI,0),42,ins(PUSH,0,0),ins(POP,1),ins(H)),5,'r1 =0x0000002a','r15=0x00003000')
# call/ret: CALL target at 0x1010; return to HALT at 0x1010? layout movi8 call8 halt4 function movi8 ret4
call_target=BASE+20
add('CALL/RET',words(ins(MOVI,15),0x3000,ins(CALL),call_target,ins(H),ins(MOVI,0),42,ins(RET)),5,'r0 =0x0000002a','r15=0x00003000','state=halted')

ok=True
for c in cases: ok=run_case(*c) and ok
print(f'\n{sum(run_case(*c) for c in []) if False else len(cases)} cases;', 'PASS' if ok else 'FAIL')
raise SystemExit(0 if ok else 1)
