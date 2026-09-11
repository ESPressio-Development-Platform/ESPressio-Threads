"""Actual dependency headers/README and negative static-composition contracts."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile
parser=argparse.ArgumentParser()
parser.add_argument('--dependencies',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-fno-rtti',f'-I{root / "src"}']
flags += [f'-I{args.dependencies.resolve() / ("ESPressio-"+name) / "src"}' for name in ['System','Task','Timing']]
with tempfile.TemporaryDirectory() as directory:
    temporary=Path(directory)
    def compile(name,source,expected=None):
        path=temporary/(name+'.cpp');path.write_text(source)
        result=subprocess.run(flags+['-c',str(path),'-o',str(temporary/(name+'.o'))],capture_output=True,text=True)
        if expected is None:
            if result.returncode: raise RuntimeError(result.stderr)
        elif result.returncode==0 or expected not in result.stderr:
            raise RuntimeError('Missing expected rejection '+name+'\n'+result.stderr)
        print(name+' passed',flush=True)
    for index,code in enumerate(re.findall(r'```cpp\n(.*?)```',(root/'README.md').read_text(),re.S)):
        compile('readme_'+str(index),code+'\nint main(){}\n')
    for path in sorted((root/'src').glob('*.hpp')):
        compile(path.stem,f'#include <{path.name}>\nint main(){{}}\n')
    prefix='#include <ESPressio_ThreadWith.hpp>\n#include <ESPressio_Precision.hpp>\nusing namespace ESPressio::Threads;\n'
    compile('empty_host',prefix+'ThreadWith<> invalid;', 'must be nonempty')
    compile('invalid_descriptor',prefix+'ThreadWith<int> invalid;', 'Invalid Thread capability protocol')
    compile('duplicate_tag',prefix+'struct A:ThreadCapability { using CapabilityTag=int; }; struct B:ThreadCapability { using CapabilityTag=int; }; ThreadWith<A,B> invalid;', 'Duplicate CapabilityTag')
    compile('exclusive_role',prefix+'struct A:ThreadCapability { using CapabilityTag=int; using ExclusiveClaims=CapabilityClaims<ApplicationCadenceRole>; }; ThreadWith<A,Precision<4>> invalid;', 'conflicting exclusive role')
    compile('physical_capability',prefix+'struct A:Thread,ThreadCapability { using CapabilityTag=int; }; ThreadWith<A> invalid;', 'must not inherit a physical Thread')
    compile('absent_access',prefix+'struct W:ThreadWith<Precision<4>> { void test(){ GetCapability<int>(); } };', 'Requested CapabilityTag is not installed')
    compile('precision_capacity',prefix+'Precision<0> invalid;', 'positive fixed telemetry capacity')
    compile('untyped_clock_rejected',prefix+'int main() { int nonmonotonic=0; Thread invalid({},&nonmonotonic); }', 'no matching function')
    compile('external_storage_overflow',prefix+'struct A:ThreadCapability { using CapabilityTag=int; static constexpr std::size_t ExternalStorageBytes=SIZE_MAX; }; struct B:ThreadCapability { using CapabilityTag=double; static constexpr std::size_t ExternalStorageBytes=1; }; ThreadWith<A,B> invalid;', 'external storage total overflows')
