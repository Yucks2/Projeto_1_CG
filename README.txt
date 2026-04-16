Um simulador de futebol dinâmico desenvolvido em C++ e OpenGL, que combina inteligência artificial tática, física de colisões e um mini-jogo especial de "invasão de campo".

SOBRE O PROJETO

Este projeto foi criado para o primeiro projeto de computação gráfica. O simulador apresenta uma partida de futebol onde 20 jogadores de linha e 2 goleiros interagem de forma autónoma. O usuário pode controlar a bola diretamente ou ativar o modo invasão, onde assume o papel de um torcedor que deve fugir dos seguranças.

FUNCIONALIDADES PRINCIPAIS

- IA tática: jogadores divididos por funções (zagueiros, meios e atacantes) com comportamentos de posicionamento e perseguição distintos.
- Física: sistema de colisões baseado em círculos com conservação de momento e resolução de sobreposição baseada em massa.
- Torcida: geração procedural de torcedores organizados por setores e cores (Estádio dividido).
- Mecânica de invasão (tecla 'I'): pausa a partida principal;
- Um torcedor aleatório entra no campo;
- O usuário controla o invasor;
- IA de perseguição para os seguranças.
- Som: efeitos sonoros integrados via "miniaudio".

TECNOLOGIAS UTILIZADAS

Linguagem: C++
Gráficos: OpenGL com GLAD
Janelamento e input: GLFW
Áudio: miniaudio
Matemática: geometria analítica e álgebra linear (teorema de pitágoras, normalização de vetores).

DETALHES TÉCNICOS

Ciclo de vida do jogo: O projeto utiliza DeltaTime para normalizar a velocidade das animações e da física, garantindo que o jogo corra à mesma velocidade em monitores de 60Hz ou 144Hz.
Máquina de estados: O programa alterna entre dois estados principais:
1. IA de futebol ativa, controle da bola pelo utilizador.
2. INVASÃO: IA de futebol pausada, controle do personagem invasor, IA de seguranças ativa.
Física de colisões: A função "resolveCollisionWithMass" utiliza o vetor normal entre dois círculos para afastar entidades que se sobrepõem, aplicando um fator de massa para determinar qual objeto tem mais "inércia" no impacto.

COMANDOS

Setas: controla a bola (modo normal) ou o invasor (modo invasão)
I: inicia a invasão de campo
Esc: fecha o simulador

ESTRUTURA DE ARQUIVOS

Para garantir que o simulador compile e execute em qualquer máquina Windows com MinGW sem necessidade de configurações complexas, o projeto inclui todas as suas dependências locais:

Código e lógica:
- Main.cpp: código-fonte principal (game loop, inteligência artificial, física e renderização).
- miniaudio.h: biblioteca de áudio responsável por tocar os efeitos sonoros.

Dependências Gráficas e de Janela (OpenGL / GLFW):
- glad.c e pasta glad: arquivo de implementação e cabeçalhos do carregador de extensões do OpenGL.
- GLFW: pasta contendo os cabeçalhos (.h) da biblioteca de janelamento e input.
- KHR: pasta contendo as definições de plataforma numérica exigidas pelo GLAD.
- libglfw3dll.a: biblioteca de ligação dinâmica (usada pelo compilador no momento do build).
- glfw3.dll: biblioteca dinâmica de tempo de execução (obrigatória na mesma pasta do .exe para o jogo abrir).

COMO COMPILAR E EXECUTAR

Este projeto utiliza ligação dinâmica com a biblioteca GLFW e requer o carregador GLAD. Todos os arquivos de dependência necessários já estão inclusos nesta pasta para facilitar a compilação (pastas "glad", "KHR" e "GLFW").

Pré-requisitos
- Compilador MinGW (g++) instalado e configurado nas variáveis de ambiente ("PATH").

Compilação (via terminal/PowerShell)
Para compilar o projeto do zero, abra o terminal nesta pasta e execute os dois comandos abaixo em sequência:

1. Gerar o arquivo objeto principal:
g++ -c Main.cpp -o Main.o -I.

2. Compilar e linkar o executável final:
g++ -o Main.exe Main.o glad.c -I. -DGLFW_DLL libglfw3dll.a -lopengl32 -lgdi32

3. Após a compilação ser concluída sem erros, inicie o jogo com o comando:
./Main.exe
