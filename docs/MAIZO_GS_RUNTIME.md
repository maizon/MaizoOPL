# MAIZO In-Game PRO: filtros de desenho — ponte opt-in

## Estado de implementação

O In-Game já oferece filtros **de saída de vídeo** (Sharp/Smooth/Deflicker),
condicionados ao modo GSM/GS suportado. Eles não equivalem a alterar o
processamento de textura ou anti-aliasing dos triângulos de um jogo.

`maizo_gs_runtime.c` integra os transformadores GIF ao estado do menu
In-Game sem expor controles falsos. É uma *ponte* de política para um
renderizador que **já possua um hook de pré-envio real e seguro**.

- Sem hook verificado: `mzGsRuntimeCaps()==0`, o menu mostra AA1,
  Bilinear e Dither como `INDISP.` e não permite modificá-los.
- Um adaptador verificado dentro da região protegida EE do MAIZO registra
  as capacidades realmente suportadas chamando `mzGsRuntimeAttach(caps)`.
  **O repositório ainda não fornece adaptadores de jogos comerciais.**
- Para cada transferência cujo buffer pertence ao adaptador, ele deve chamar
  `mzGsRuntimeSubmit(...)` **antes da DMA ou de copiar para VU1**. Sem
  acesso exclusivo, sem sincronização de cache ou com transferências ativas,
  o adaptador deve encaminhar o pacote original sem alterações.
- Os modos por filtro são `JOGO` (preservar futuras escritas), `ON`
  (forçar bit/registrador quando compatível) e `OFF` (forçar desligar).
  Alternar para JOGO não restaura retroativamente registradores GS já
  enviados: a próxima reprogramação do jogo determina o resultado.
- Após três pacotes inválidos consecutivos, o registro é desativado
  automaticamente. Em jogos sem hook, nenhuma interceptação acontece.
- A inicialização de novo jogo redefine explicitamente as capacidades.

## API de integração

Cabeçalho: `ee_core/include/maizo_gs_runtime.h`

- `mzGsRuntimeReset()`: limpa estado no carregamento de um jogo.
- `mzGsRuntimeAttach(MZ_DRAW_*)`: **somente** após reconhecimento e
  instalação de hook com segurança verificada. Não chamar apenas porque
  o jogo possui GIF/DMA, nem para exibir opções do menu.
- `mzGsRuntimeCaps()`, `mzGsRuntimeMode()`,
  `mzGsRuntimeSetMode()`: estado de menu válido para o hook anexado.
- `mzGsRuntimeSubmit()`: dispatch de PATH3 normal/CNT/REF,
  PATH2 VIF1 DIRECT/DIRECTHL e buffers staging para PATH1 (não é
  interceptação XGKICK). Conserva as restrições dos adaptadores.

O processamento mantém custo linear no tamanho do comando GIF, com
validação prévia em duas passagens, e **não** constitui promessa de
FPS estável sem benchmark em hardware real. Preservar originais se for
necessária reversão byte a byte de buffers reutilizados.

## Compatibilidade e meta de 90%

O PS2 não possui shaders pós-processados de PCSX2 nem a potência extra
da GPU hospedeira. AA1 do GS não é MSAA/FXAA moderno, e a filtragem
bilinear não aumenta resolução interna.

**Não existe prova de 90% de cobertura em jogos.** Sem testes no
console, o valor correto de compatibilidade medida continua
`0 jogos comprovadamente suportados`, não `0% de jogos compatíveis`.

Para declarar um jogo compatível, precisamos (1) capturar o fluxo real
com exclusividade, (2) enviar alterações antes da DMA, (3) verificar
visualmente AA1/TEX1/DTHE, (4) testar pausa/retorno e ON/OFF,
(5) verificar regressões em sessões longas e gameplay com o controle
USB/DS4 e áudio. Cada versão/região do ELF pode exigir nova verificação.

## Testes

`tests/maizo_igr_state_test.c`: opções desabilitadas até a ligação.
`tests/maizo_gs_runtime_test.c`: registros opt-in, aplicação nos três
caminhos quando fornecidos pelo adaptador, e desativação automática
após três falhas. Não substituem testes em PS2 físico.
