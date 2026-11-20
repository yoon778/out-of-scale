# 학교에서 군대로 이어지는 통합 검사

저장소 루트에서 실행한다. Visual Studio 2026 C++ 도구와 로컬 OpenGL 라이브러리가 필요하다.

```powershell
./project_code/Out_of_Scale/tests/run-school-transition.ps1
```

경로가 다르면 `-VisualStudioRoot`와 `-OpenGLRoot`를 지정한다. 스크립트는 팀 Release를 `WholeProgramOptimization=false`로 빌드한 뒤 실제 개체 파일과 검사 드라이버를 링크한다. 프로젝트 설정은 바꾸지 않는다. 로그와 캡처는 `build/school-transition-tests/`에 저장한다.

검사는 숨김 OpenGL 창에서 실제 `runGame`, `runPreview`, 공통 Player와 군대 방을 사용한다. 키·시간·포커스만 고정 입력으로 대체하며 모니터 전환을 막는다. 카메라를 학교 현관 직전으로 배치한 뒤 실제 W 입력으로 완료 판정을 통과한다. 완료 플래그를 직접 설정하지 않는다. 그 이전 퍼즐 동선은 학교 실행기의 기존 `--self-test`에서 별도로 검사한다.

- 학교 정지 중 이동·전환 차단, 재개 후 단축키 없는 생활관 도착.
- 생활관 도착 위치·방향, 계속 누른 W 이동, 도착 후 공용 일시정지.
- F1으로 완료한 학교 재방문, 자동 재전환 방지, F2 복귀 시 군대 위치 보존.
- 학교 단독 실행의 완료 화면 유지, 입력 문맥 정리, OpenGL 오류.

자동 입력 검사이며 사람의 직접 조작감 평가는 아니다.
