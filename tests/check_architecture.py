"""Check active source boundaries; historical migration records may name removed APIs."""
from pathlib import Path
import json
import re
root=Path(__file__).resolve().parents[1]
source='\n'.join(p.read_text() for p in (root/'src').glob('*') if p.is_file())
forbidden=[r'vTaskSetThreadLocalStoragePointer',r'pvTaskGetThreadLocalStoragePointer',r'freertos/',
           r'TaskHandle_t',r'xTaskCreate',r'vTaskDelete',r'TaskRuntime::(?:Delete|Suspend)',
           r'ThreadManager',r'ThreadTerminationDispatcher',r'PrecisionThread',r'ReleaseOnTerminate',
           r'std::(?:function|vector|deque|map)',r'ESPressio_(?:Event|Command|State|Observable|Radio|Mesh|Adapter)']
for pattern in forbidden:
    assert not re.search(pattern,source), 'Forbidden source dependency/mechanism: '+pattern
manifest=json.loads((root/'library.json').read_text())
assert {x['name'] for x in manifest['dependencies']}=={'ESPressio-System','ESPressio-Task','ESPressio-Timing','ESPressio-Units'}
assert all(x['version'].endswith('#primitives_redesign') for x in manifest['dependencies'])
print('Active source dependency/predecessor/TLS boundaries passed')
