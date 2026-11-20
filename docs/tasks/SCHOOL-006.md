# SCHOOL-006 — 학교 현관과 군대 생활관 연결

## 작업 시작

- 날짜 / 담당자 / 상태: 2026-11-20 / 김새누 작업 지원 Codex / 구현·자동 검증 완료, 인계 가능.
- 시작 기준: `saenu` `af3ce16`, 깨끗한 작업 트리. `origin/main`·`origin/saenu`도 같은 커밋이다.
- 요청: 새누 담당 방을 탈출하면 윤서 담당 방으로 자연스럽게 이어지게 한다. 사용자가 작업 전 AI 지침 문서 확인도 명시했다.
- 확인: `AGENTS.md`, `docs/ai-collaboration-rules.md`, `docs/TEAM.md`, 공식 기획서의 학교·군대 순서와 연결 규칙, SCHOOL-005·GIT-20261120-01, 공유 B-016·B-026·최신 B-027을 확인했다. 최신 윤서 원격은 `0aec6b6`이며 마지막 커밋은 진단 기록만 변경했다.
- 목표 / 완료 기준: 통합 실행에서 학교 현관을 걸어 나오면 단축키 없이 군대 생활관으로 이동한다. 정지 중에는 전환하지 않으며, 단독 학교 실행과 F1~F4 개발 전환·완료한 학교 재방문을 유지한다. 실제 게임 반복 루프와 기존 학교 검사를 실행한다.
- 범위 / 의존 관계: 학교 반복 루프와 공통 진입점의 기존 학교 분기 내부를 연결한다. 기존 `runPreview` 반환·`escaped()`·`PreviewNavigation` 계약을 사용하며 공개 함수 서명·빌드 설정·군대/내 방 소스는 변경하지 않는다. 윤서의 아직 main에 없는 컴퓨터 입력 변경과 수정 구간을 분리한다.

## 진행·인계

- 학교가 미완료에서 완료로 바뀌는 순간 통합 호출자에게 반환한다. 단독 실행에는 호출자가 없어 기존 완료 화면을 유지한다. 이미 완료된 학교를 F1으로 다시 방문한 경우 즉시 튕겨 나가지 않는다.
- 공통 진입점은 단축키 요청을 우선 처리하며, 정상 학교 완료는 기존 군대 생활관 시작 위치·방향으로 연결한다. 기존 군대 퍼즐 상태를 유지하고, 방 반복 루프의 마우스 기준·이동 속도 정리와 생활관 복귀 동작을 재사용한다.
- 수정 파일: `rooms/school/SchoolPreview.cpp`, `project_code/Out_of_Scale/main.cpp`, `rooms/school/README.md`, `project_code/Out_of_Scale/tests/SchoolTransitionTests.cpp`·`run-school-transition.ps1`·`README.md`, 이 기록.
- 검증: 팀 Debug·Release x64 빌드 통과. Release는 기존 검증 방식대로 명령에만 `WholeProgramOptimization=false`를 지정했다. 학교 Release 빌드·기존 474개 검사 통과, 실패 0.
- 통합 실행: 숨김 OpenGL에서 실제 `runGame`·`runPreview`·Player·군대 구현을 사용한 **21개 검사 통과, 실패 0**. 학교 현관 직전 좌표에서 W로 걸어 실제 완료 판정을 통과하고 생활관 도착 위치·방향·이동 지속, 출발지와 도착지의 공용 정지, F1 완료 학교 재방문, F2 군대 재방문 위치 보존, 단독 학교 완료 유지와 입력 정리를 확인했다. 완료 플래그를 직접 설정하지 않았다.
- 화면: 자동 전환 이후 생활관 화면 `build/school-transition-tests/military-arrival.png`를 직접 확인했다. OpenGL 오류 0. 통합 테스트는 현관 직전부터 시작하며, 앞선 퍼즐 완주는 기존 학교 474개 검사에서 별도로 확인한다.
- 미검증: 사람의 직접 키보드·마우스 체감, 학교 단독 미리보기에서 군대로 이동하는 기능(의도적으로 팀 통합 실행에만 연결), 다른 GPU. 기본 LTCG Release 빌드는 이번 검증 대상이 아니다.
- 상대 담당 영향: 기존 공용 인터페이스 내부의 연결 구현이다. 군대·내 방 소스는 읽기만 했으며 다른 개발자의 진행 중 변경을 병합하지 않는다.
- Git: 사용자의 상시 기능별 커밋 지시에 따라 검증한 7개 파일과 이 기록을 하나의 연결 기능 커밋으로 남긴다. 푸시·main 병합은 이번 후속 작업에서 미수행이다.

## AI 활용

- 사용자 요청과 공식 기획서의 방 순서를 대조하고 기존 반환 경로를 연결했다. 사람이 요구한 문서 선확인을 수행했으며, 구현 결과의 사람 검토는 미확인이다.

## 최종 확인·재현

- `./project_code/Out_of_Scale/tests/run-school-transition.ps1`: 팀 Release 빌드와 현재 개체 파일을 연결한 숨김 실행 검사. 로그와 도착 화면은 `build/school-transition-tests/`에 저장한다.
- `./rooms/school/preview/bin/Release/SchoolPreview.exe --self-test`: 학교 퍼즐과 교실부터 현관까지의 기존 자동 검사.
- 실제 플레이는 `project_code/Out_of_Scale/x64/Debug/Out_of_Scale.exe` 또는 Release 구성의 통합 실행 파일을 사용한다. 두 실행 파일을 갱신했다.
- 커밋 직전 재확인: 공유 main/saenu `af3ce16`, yoon778 `0aec6b6` 및 관련 MD에 추가 변경 없음. 윤서 브랜치 main.cpp의 컴퓨터 입력 변경과 이번 학교 반환 처리 구간은 겹치지 않는다. 상대 담당 코드는 변경하지 않았다.
- 문서 UTF-8·상대 링크·변경 범위·공백 확인 완료. 남은 구현 작업 없음. 결과의 사람 검토와 원격 공유는 별도다.

## 2026-11-20 공유 완료

- 사용자 main 적용 요청으로 `f045413`을 포함한 `99a53ba`까지 main에 fast-forward 병합하고 origin/main·origin/saenu에 푸시했다. 원격 커밋 일치 확인. 앞의 미공유 문구는 당시 상태다. 상세: [GIT-20261120-02](GIT-20261120-02.md).
