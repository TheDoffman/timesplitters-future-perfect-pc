"""Decode only field-access facts from the private supported-US title."""
from pathlib import Path
import hashlib,sys
import pytest
from capstone import Cs,CS_ARCH_X86,CS_MODE_32
from capstone.x86_const import X86_OP_MEM,X86_OP_IMM,X86_REG_EAX,X86_REG_EBX,X86_REG_ECX,X86_REG_EDI,X86_REG_ESI
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
from xbe_image import XBEParser

@pytest.fixture(scope='module')
def original():
    path=ROOT/'private/game/default.xbe'
    if not path.is_file():pytest.skip('Private supported-US title required')
    assert hashlib.sha256(path.read_bytes()).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    parser=XBEParser(path);parser.parse();decoder=Cs(CS_ARCH_X86,CS_MODE_32);decoder.detail=True
    return parser,decoder

@pytest.mark.parametrize('address,mnemonic,index,base,offset,size',[
    (0x46eef1,'mov',1,X86_REG_EAX,0x5c,4),
    (0x46eef4,'mov',1,X86_REG_ECX,0x0c,4),
    (0x46eef7,'mov',1,X86_REG_ECX,0x04,4),
    (0x46ef14,'adc',1,X86_REG_ECX,0x10,4),
    (0x46ef35,'test',0,X86_REG_ECX,0x02,1),
    (0x46ef3b,'mov',1,X86_REG_ECX,0x08,4),
    (0x46ef49,'mov',1,X86_REG_EAX,0x30,4),
    (0x46ef4c,'add',1,X86_REG_ECX,0x08,4),
    (0x46f080,'mov',1,X86_REG_EDI,0x5c,4),
    (0x46f0aa,'mov',0,X86_REG_ESI,0x14,4),
    (0x46f0b3,'movzx',1,X86_REG_ESI,0x00,1),
    (0x46e5a9,'or',0,X86_REG_EAX,0x03,1),
])
def test_supported_title_field_accesses(original,address,mnemonic,index,base,offset,size):
    parser,decoder=original
    ins=next(decoder.disasm(parser.at(address,15),address,count=1))
    assert ins.address==address and ins.mnemonic==mnemonic
    operand=ins.operands[index]
    assert operand.type==X86_OP_MEM and operand.size==size
    assert operand.mem.base==base and operand.mem.index==0 and operand.mem.disp==offset
    if address==0x46ef35:assert ins.operands[1].type==X86_OP_IMM and ins.operands[1].imm==0x80
    if address==0x46e5a9:assert ins.operands[1].type==X86_OP_IMM and ins.operands[1].imm==1
    if address==0x46f0aa:assert ins.operands[1].reg==X86_REG_EBX
