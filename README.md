# STM32 RC 자동차 제어

Bluetooth 조이스틱 명령으로 좌우 모터를 제어하고, 초음파 거리 기반 자동 주행을 팀 프로젝트로 구성하였습니다.

- 수행 시기 2026.11
- 참여 형태 2인 팀
- 담당 범위 제어 앱 설계, Bluetooth 통신 및 연속 수동 각도 제어

[포트폴리오 상세 페이지](https://eugeneson3.github.io/projects/stm32-rc-car/)

## 프로젝트 목표

Bluetooth 기반 수동 주행과 초음파 센서 기반 자동 주행을 구현하고, 통신 입력을 모터 제어에 연결하는 것을 목표로 하였습니다.

## 설계 및 구현

- 본인은 RC 제어 앱, Bluetooth 통신 및 연속 수동 각도 제어를 담당하였습니다. 팀원 이도성은 자동 주행과 거리 측정 모듈을 담당하였습니다.
- 수동 제어 소스는 X·Y 숫자 필드를 해석하고 Queue를 통해 모터 Task에 전달합니다.
- LCD 접근에는 Mutex를 사용하며 통신·버튼·모터 작업을 분리하였습니다.
- 추가 자율주행 버전에는 센서 순차 측정, 유효 시간 검사, 회전 유지 시간과 진입·복귀 임계값을 구성하였습니다.

## Troubleshooting

### 첫 명령 이후 통신 정지

한 번 명령을 보낸 뒤 후속 명령이 처리되지 않는 현상을 관찰하였습니다.

발표 자료는 문자열 변환 및 LCD 호출 과정의 CommTask 스택 부족을 추정 원인으로 기록하고 있습니다.

발표에서 스택을 128워드에서 512워드로 확대하였습니다.

발표 기록에서 후속 명령 처리가 회복되었습니다. 공개된 manual-rtos 스냅샷의 설정은 128워드이므로 수정 전 버전으로 명시하였습니다.

### 조이스틱 해제 시 정지 명령 유실

앱에서 한 번 전송한 STOP 명령이 반영되지 않는 경우가 있었습니다.

단발 명령 전달 방식에서 정지 상태를 지속적으로 전달할 필요가 있었습니다.

발표 자료에 조이스틱 해제 상태에서 STOP 명령을 반복 전송하도록 변경한 내용을 기록하였습니다.

수동 조작 영상과 개선 영상을 발표 자료에 수록하였습니다.

## 검증 및 결과

- 수동 주행 및 자동 주행 시연 영상이 존재합니다.
- 저장소에서 수동 RTOS 제어, 기본 자동 주행, 최대 듀티 자동 주행의 소스를 각각 확인할 수 있습니다.
- 2026.11.18 autonomous-maxspeed의 기존 Arm GCC·Unicorn 테스트를 실행하여 상태 전이, 시간 경계, PWM 및 센서 timeout 검사를 통과하였습니다.

## 소스코드 구조

각 폴더는 서로 다른 개발 스냅샷입니다. 발표 최종 통합본과 모바일 앱 원본은 확인되지 않았습니다. autonomous-maxspeed의 최대 속도는 PWM 듀티 100%를 의미하며, 해당 추가 버전의 실차 주행 성능은 별도 확인이 필요합니다.

| 경로 | 내용 |
| --- | --- |
| [manual-rtos/Core/Src/ap/task/comm_task/](manual-rtos/Core/Src/ap/task/comm_task/) | Bluetooth 명령 파싱 및 전달 |
| [manual-rtos/Core/Src/freertos.c](manual-rtos/Core/Src/freertos.c) | Task·Queue·Mutex 생성 |
| [autonomous/Core/Src/main.c](autonomous/Core/Src/main.c) | 기본 초음파 자동 주행 |
| [autonomous-maxspeed/Core/Src/navigation.c](autonomous-maxspeed/Core/Src/navigation.c) | 거리·유효 시간 기반 방향 판단 |
| [autonomous-maxspeed/tests/](autonomous-maxspeed/tests/) | C 코드 기반 펌웨어 테스트 |

## 실행 방법

1. 각 폴더를 별도 STM32CubeIDE 프로젝트로 Import합니다.
2. 해당 폴더의 .ioc와 배선 설명을 기준으로 하드웨어를 연결합니다.
3. manual-rtos 명령은 *로 종료됩니다. 자동 주행 추가 버전은 자체 README와 tests/run_tests.py에 실행 절차가 있습니다.

## 동작 동영상

- [Bluetooth 수동 주행](https://www.youtube.com/watch?v=PSv0RurP0Og)
- [팀 자동 주행 시연](https://www.youtube.com/watch?v=nSet8mlpK6E)

## 참고 자료

- [발표 자료](https://docs.google.com/presentation/d/1PTB3ovCwl-OSSKuT2UqVhh27Gt-OEVEujULe4K4Zsnk/edit)

## 소스 이력

2026년 11월 18일 로컬 프로젝트에서 구성한 공개 스냅샷입니다. 원본 파일의 상대 경로와 SHA-256은 [SOURCE_MANIFEST.json](SOURCE_MANIFEST.json)에 기록하였습니다. 빌드 산출물과 로컬 개발 환경은 제외하였습니다. 파일별 저작권 및 라이선스 고지를 유지하였습니다.
