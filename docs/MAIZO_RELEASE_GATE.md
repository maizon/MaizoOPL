# MAIZO: gate de release experimental

Execute `python3 scripts/maizo_release_gate.py <SHA-completo> --report hardware.json` com GitHub CLI autenticado.

O script consulta CI e artefatos reais do SHA indicado. Sem relatório físico retorna falha, mesmo com compilação perfeita. Não modifica o repositório, não inventa evidências e não faz testes físicos por conta própria.

Para cada ID listado na saída, o relatório JSON deve conter o SHA exato em `commit` e uma entrada em `tests` com `id`, `result` (pass/fail), `console` (modelo e configuração) e `evidence` (observação concreta, vídeo, log ou referência de teste). O teste deve usar a build desse SHA. A aprovação depende da veracidade do relatório; o programa não autentica o vídeo ou a observação.

Critérios: abrir durante gameplay e repetir; atravessar transições e carregamentos depois do menu; observar filtros em jogos comerciais reais; ler o ZIP original por SMB e executar códigos simples/condicionais/complexos; gravar sem sair e confirmar após reboot; confirmar geometria na primeira tela com GSM e nativa. Nenhum recurso é considerado universal só porque o lab ou o compilador passou.

Enquanto não houver evidência para todos os critérios, o artefato continua experimental. O gate não substitui a proteção de memória ou os testes de regressão existentes.
