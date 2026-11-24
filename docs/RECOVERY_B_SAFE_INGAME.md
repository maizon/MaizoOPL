# Reintegração segura da Correção B — In-Game PRO

**Base conhecida:** Beta-82 (855d948), mas com exatamente os 6
arquivos de núcleo/launch da Beta-39 restaurados na Correção B
(de9ad9d). A interface, MAIZO Store, drivers USB HID/Xbox/DS4 e
demais recursos externos à EE core seguem baseados na Beta-82.

**Problema confirmado por medição de linker:**
- Beta-39 `_end_bss=0x93900`, pilha **14.080 B**.
- Beta-82 `_end_bss=0x96D40`, pilha **704 B**.
- São números de compilação; a causa da tela preta ainda depende de
  validação física. A pilha é usada por `SetupThread` em `crt0.S`.

## Estratégia de integração

Há duas builds, explicitamente distintas:
1. **Padrão / segura**: mantém binariamente os fontes de núcleo/GSM/IGR
   usados na Correção B. O seletor de In-Game PRO no frontend informa
   que ele não está ativo; não engana o jogador com um ON falso. Teste
   de linker falha se a pilha da release cair abaixo de 8192 B.
2. **`MAIZO_IGR_EXPERIMENTAL=1`**: habilita os arquivos separados
   `maizo_gsm_engine.S`, `maizo_gsm_api.c`, menu categorizado,
   GS observador sem forçar GSM, diagnóstico de PATH e filtros
   condicionados a compatibilidade. O código experimental NÃO substitui
   os arquivos da Correção B na build normal. O verificador recusa
   compilações da release se a pilha ficar menor que 8 KiB.

Os seguintes itens estão preservados em código, mas NÃO se tornam
necessariamente funcionais na build padrão: menu In-Game em jogos,
Sharp/Smooth/Deflicker do menu, 480p framebuffer => 480i,
AA1/Bilinear/Dithering e diagnósticos GS. O filtro
AA1/Bilinear/Dither requer um adaptador por jogo que entregue pacotes
GIF/VIF com buffers de propriedade verificada. Não existe hook
universal comercial validado. A disponibilidade depende de hardware,
modo de vídeo e jogo.

**Nunca usar a flag experimental como versão universal estável
sem teste real.** Se a build experimental ultrapassar a memória
permitida, o CI deverá falhar explicitamente, nunca lançar uma
imagem com 704 B de pilha. A versão segura fica intocada.

Como reproduzir:
```sh
make clean
make
# em uma árvore limpa e separada:
make MAIZO_IGR_EXPERIMENTAL=1
```

Variantes IGS/debug têm restrições próprias; a build experimental
só é suportada na configuração release sem IGS.

Nenhum dos testes automatizados simula o GS, DMA, kernel e controle
de um PS2 real. A Correção B não teve validação física comunicada.
