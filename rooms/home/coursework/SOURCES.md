# 컴퓨터 안의 수업 실습

학생이 작성한 기존 실습을 게임의 한 OpenGL 문맥·FBO 안으로 이식
원본 폴더와 원본 파일은 수정하지 않음

원본 공통 경로: `C:/Users/cys04/Desktop/My_workspace/Knowledge/My Second Brain/10 University/2_2_computergrapics/코딩/coding/coding/`

| 화면 | 원본 | 유지한 요소 |
| --- | --- | --- |
| 도형 만들기 | `practice_1_7/1_7.cpp`, `vertex.glsl`, `fragment.glsl` | P/E/T/R 생성, 클릭 선택, 이동, 색과 도형 크기, 경계 |
| 사각형 쌓기 | `practice_1_12/1_12.cpp`, 동명 GLSL | 상하 왕복, 파란 점선, Enter 판정, 탑 이동·누적, R |
| 3D 변환 | `practice_1_14/1_14.cpp`, 동명 GLSL | 면 좌표·꼭짓점 색, 고정 축, X/Y 회전, 이동, 깊이·와이어 |
| 행성과 달 | `practice_1_18/1_18.cpp`, `sphere.obj`, 동명 GLSL | 실제 구 메쉬, 160분할 궤도, 행성·달 계층 변환, 투영·크기·이동 |
| 빛과 도형 | `practice_1_26/1_26.cpp`, 동명 GLSL | 면 법선, 96분할 광원 궤도, 원본 Phong 식·카메라·조작 |
| 열쇠 제작 | 신규 | 4개 도형의 마우스 조립·Q/R 회전·휠 크기, 완성품만 E 출력 |

`sphere.obj`는 18번 원본 그대로 복사
프로젝트 빌드 시 실행 파일 옆 `rooms/home/coursework/`에도 복사하여 실행 폴더에서 읽을 수 있게 구성
이식한 셰이더 식은 CourseworkLabs.cpp에 포함하여 실행 시 외부 GLSL 경로 의존성 제거
2D 실습은 같은 프로그램의 조명 꺼짐 경로를 사용하며 좌표·색 처리는 원본과 같음
수업 창 종료·생성 코드는 복사하지 않음. 원본 Q/Esc 종료 대신 게임의 Tab/Esc 동작 사용
1~5번 도형은 게임 방으로 꺼낼 수 없음

화면 차이: 4:3 실습 영역을 고정하고 수업 목록·조작 안내 추가, 난수 도형 색/위치는 재현 가능한 난수열, 12번 탑 색은 고정 팔레트
1_12/1_14의 비어 있던 마우스 콜백에 새 드래그 기능을 넣지 않음
