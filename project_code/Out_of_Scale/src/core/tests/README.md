# 공통 Player 이동 검사

저장소 루트의 Developer PowerShell에서 실행한다. 실제 `Player.cpp`를 컴파일하며 GLFW의 키·포커스 조회만 고정 입력으로 대체한다. 창·렌더링·실제 키보드 이벤트 검사는 아니다. 팀 프로젝트의 빌드 설정에는 연결하지 않는다.

```powershell
New-Item -ItemType Directory -Force build/player-tests | Out-Null
cl /nologo /std:c++20 /EHsc /W4 /MD /O2 /I "$env:USERPROFILE/source/libs/OpenGL/include" project_code/Out_of_Scale/src/core/Player.cpp project_code/Out_of_Scale/src/core/tests/PlayerTests.cpp /Fo:build/player-tests/ /Fe:build/player-tests/PlayerTests.exe
if ($LASTEXITCODE -ne 0) { throw 'Player 검사 빌드 실패' }
& ./build/player-tests/PlayerTests.exe
```

20·30·60·144 FPS에서 가속·최고 속도·정지 거리·이동 거리, 대각선 정규화, 방의 충돌 보정 뒤 벽을 따라 이동하기, 포커스·맵 재진입 시 잔여 속도 제거, 즉시 마우스 시점과 점프 착지를 확인한다. `OpenGL` 헤더가 다른 곳에 있다면 `/I` 경로만 변경한다.
