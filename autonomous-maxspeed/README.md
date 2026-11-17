# SR04 센서 3개를 사용하는 최대속도 자율주행

STM32F411RE / STM32CubeIDE 프로젝트입니다. 기존 실습의 8 MHz HSE, 100 MHz 시스템 클럭과 DC 모터 배선을 사용합니다.
기존 프로젝트와 별도로 가져올 수 있도록 프로젝트 이름과 `.ioc`, 빌드 경로를 모두 변경했습니다.

## 배선

| 센서 | 방향 | TRIG | ECHO |
| --- | --- | --- | --- |
| 왼쪽 | 왼쪽 대각선 | PA8 | PB10 |
| 가운데 | 정면 | PB4 | PB5 |
| 오른쪽 | 오른쪽 대각선 | PA10 | PB3 |

| 기능 | 핀 |
| --- | --- |
| 시작/정지 버튼 | PC0, 누르면 GND에 연결, 내부 Pull-up |
| 왼쪽 모터 EN/PWM | PC6 / TIM3 CH1 |
| 왼쪽 모터 IN1 / IN2 | PB6 / PA7 |
| 오른쪽 모터 EN/PWM | PC8 / TIM3 CH3 |
| 오른쪽 모터 IN1 / IN2 | PA6 / PA5 |

각 모터는 IN1=High, IN2=Low일 때 차량 전진 방향으로 돌아가도록 배선합니다.
SWD 디버깅을 사용하며 PB3/PB4는 센서 GPIO로 사용하므로 SWO/JTAG와 공유하지 않습니다.
센서/보드/모터 드라이버의 GND를 공통 연결합니다. 5 V SR04의 ECHO는 3.3 V 레벨 변환 또는 분압을 거쳐 표의 입력 핀에 연결하는 구성을 권장합니다.

## 동작

1. 전원 인가 시 모터는 정지합니다.
2. PC0을 누르면 자율주행을 시작합니다. 30 ms 디바운스로 한 번 누른 동작을 한 번만 처리합니다.
3. 시작 이후 센서 3개의 새 유효값을 모두 확보한 다음 주행합니다.
4. 정면이 60 cm 이상이고 양쪽 대각선이 각각 30 cm 이상이면 양쪽 모터 PWM 100%로 전진합니다.
5. 이 범위 안에 장애물이 있으면 왼쪽/오른쪽 대각선 거리 중 더 큰 쪽으로 제자리 회전합니다. 좌회전은 왼쪽 후진/오른쪽 전진, 우회전은 그 반대이며 두 바퀴 모두 PWM 100%입니다.
6. 거리 차이가 10 cm 이내이면 이전 회전 방향을 유지합니다. 첫 동률은 왼쪽을 선택합니다. 한 번 선택한 회전은 최소 250 ms 유지하고, 그 이후 더 넓은 쪽이 바뀌면 방향을 바꿀 수 있습니다.
7. 회전 중에는 정면 85 cm 이상, 양쪽 각각 40 cm 이상이 확보되어야 다시 전진합니다. 진입/복귀 기준을 다르게 해 경계에서 흔들리는 현상을 줄입니다.
8. PC0을 다시 누르면 정지합니다. 센서 오류나 오래된 값에 따른 정지는 최소 회전 시간과 관계없이 적용됩니다.

전진/회전 중 방향 변경 전에는 PWM을 0으로 즉시 반영하고 모든 방향 출력을 Low로 내린 뒤 3 ms 후 새 방향을 인가합니다.
최대속도는 속도 피드백 제어가 없는 **PWM 듀티 100%**를 뜻하며 실제 속도는 모터/배터리/노면에 따라 달라집니다.

## 거리 측정과 오류 처리

- 트리거 12 us, ECHO High 시간 / 58로 cm 환산합니다.
- 정면 → 왼쪽 → 정면 → 오른쪽 순서로 한 번에 하나씩 측정합니다. 트리거 사이 간격은 65 ms, 정면 갱신은 약 130 ms, 각 측면 갱신은 약 260 ms입니다.
- DWT 사이클 카운터와 논블로킹 폴링을 사용합니다. 주 루프에 `HAL_Delay`, UART 출력 등 긴 작업을 넣으면 펄스 측정을 놓칠 수 있습니다.
- ECHO 상승/하강 대기는 각각 최대 25 ms입니다. 2~400 cm 펄스만 유효값으로 사용합니다.
- 응답 없음, 측정 범위 밖, ECHO 고착, 450 ms 초과 갱신 지연은 정지 사유입니다. **4 m 밖의 개방 공간에서 ECHO가 돌아오지 않는 경우도 정지합니다.** 무응답을 무조건 빈 공간으로 간주하지 않습니다. 유효값이 회복되면 버튼으로 정지한 상태가 아닌 한 자동으로 재개합니다.
- HAL 오류와 CPU HardFault/BusFault/MemManage/UsageFault에서도 모터 출력을 끄도록 처리했습니다.

측정 타이밍은 [HC-SR04 데이터시트](https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf)의 10 us 이상 트리거와 60 ms 초과 측정 주기를 기준으로 설정했습니다. PWM 비교값과 업데이트 처리는 [STM32F411 RM0383](https://www.st.com/resource/en/reference_manual/rm0383-stm32f411xce-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)를 참고했습니다.

## 가져오기 / 빌드 / 실행

1. STM32CubeIDE에서 **File → Import → General → Existing Projects into Workspace**를 선택합니다.
2. 이 폴더를 선택하고 `20261116_SR04_Autonomous_MaxSpeed`를 가져옵니다. 소스 복사 없이 현재 위치에서 사용할 수 있습니다.
3. **Build Project** 후 보드에 **Run/Debug**로 다운로드합니다.
4. PC0으로 시작/정지합니다. 처음에는 바퀴를 들어 좌/우 모터의 전진 및 회전 방향을 확인합니다.

PowerShell에서 GUI 없이 빌드하려면 이 폴더에서 실행합니다.

```powershell
.\build.ps1
# 다른 설치 경로 또는 Release 구성
.\build.ps1 -Configuration Release -CubeIdeRoot 'C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE'
```

빌드 스크립트는 구성 폴더에 `.elf`, `.hex`, `.bin`을 만듭니다. `.bin`의 Flash 시작 주소는 `0x08000000`입니다.

## 조정 위치 / 디버깅

- `Core/Inc/navigation.h`: 전방/측방 회피 거리, 복귀 거리, 회전 여유, 최소 회전 시간, 데이터 유효 시간.
- `Core/Src/autonomous.c`: 센서 핀 배열, 측정 순서/간격, 디바운스, 모터 방향/최대 듀티.
- `Core/Src/navigation.c`: 거리 기반 주행 판단.
- `Core/Src/main.c`: GPIO/TIM3 초기화 및 주 루프 연결.
- `.ioc`: 지정 센서 6핀, PC0 Pull-up, TIM3 PWM 2채널, SWD 설정이 반영되어 있어 CubeMX 코드 재생성 시에도 유지됩니다. 사용자 코드는 USER CODE 영역과 별도 파일에 있습니다.

Live Expressions에서 `distances[0]`(왼쪽), `distances[1]`(정면), `distances[2]`(오른쪽)의 `cm`, `valid`, `measured_ms`를 확인할 수 있습니다.
`autonomous_running`은 버튼 상태, `requested_motion`은 주행 판단, `actual_motion`은 현재 모터 출력입니다. 동작 값은 0=정지, 1=전진, 2=좌회전, 3=우회전입니다.

현재 60 cm 전방 회피 거리는 초기 설정값입니다. 실제 차체의 최대속도와 측정 지연/제동 거리에 맞춰 조정해야 합니다. 실물 차량에서의 주행/회피 성능은 별도 확인이 필요합니다.

## 검증

STM32CubeIDE의 Arm GCC로 전체 펌웨어를 컴파일/링크하고, 실제 주행 C 코드를 Cortex-M4 에뮬레이터에서 실행하는 테스트를 제공합니다. GPIO/HAL/PWM은 테스트 대역이므로 실제 전기 신호와 차량 움직임까지 검증하지는 않습니다.

```powershell
py -m pip install --target Debug/test-deps unicorn
$toolBin = 'C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin'
py tests/run_tests.py --tool-bin $toolBin
```

검증 항목: 전진/좌우 장애물 판단, 회전 방향 유지/전환, 복귀 거리, 데이터 오류/지연, tick 오버플로, 버튼 디바운스/길게 누름/시작/정지, PWM 100%와 방향 전환 대기, 지정 TRIG/ECHO 매핑, 순차 측정과 거리 환산, ECHO 무응답/고착 타임아웃.
