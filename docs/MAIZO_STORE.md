# MAIZO Store — GitHub homebrews no OPL (Ethernet)

## Integração

A loja foi adicionada ao menu **Configurações → Rede → MAIZO Store - homebrews**.
Na aba Aplicativos da interface Supreme, **L3** também abre a loja.

A lista contém quatro arquivos PS2 ELF de releases verificados no GitHub em
23/11/2026: wLaunchELF, wLaunchELF ISR DS34, wLaunchELF ISR exFAT e FCEUmm.
A lista fica disponível offline. O download só habilita quando o servidor
responde e seu catálogo coincide com o SHA-256 e tamanho compilados no cliente.

## O que precisa para downloads online

O repositório https://github.com/maizon/MaizoOPLstore contém o servidor
`server.py` de catálogo e espelho verificado. Ele **PRECISA SER IMPLANTADO**
numa hospedagem com acesso público via **TCP/HTTP**; a fonte de código GitHub
não é por si só um servidor HTTP acessível em porta TCP pelo PS2.

Exemplo de instalação da parte servidor:
```sh
PORT=8080 python3 server.py
```
Use um host de Internet com proxy TCP externo mapeando a porta 8080 do app.
No PS2, entre na loja, aperte TRIÂNGULO e informe host DNS e porta pública.
O PS2 usa sua Ethernet RJ-45, sem smartphone, PC doméstico ou SMB obrigatório.

## Instalação

Os downloads ficam em `mass0:/APPS/<id>/BOOT.ELF`.
Após conferir tamanho e SHA-256, o programa substitui o ELF por renomeação
com backup `BOOT.BAK`. A pasta recebe `title.cfg` com `title` e `boot`
para reconhecimento pela biblioteca APPS original. Ao terminar, atualize a
biblioteca. Memória USB deve ser gravável e reconhecida no OPL.

A verificação criptográfica é **local**, em PS2, mesmo usando HTTP simples.
Um atacante poderia alterar nomes/status na rede ou interromper a conexão;
não consegue injetar ELF diferente sem que o cliente rejeite o SHA-256.
O cliente não aceita downloads arbitrários nem confia em hashes do servidor.
Por ser HTTP, metadados e downloads não são confidenciais. Para proteger
também o catálogo use um canal TLS compatível ou assinatura de catálogo
numa versão futura.

## Segurança e limitações

- Não foi testado em PS2 físico. Compilação CI e teste do hash são necessários.
- GitHub `latest` pode mudar: se o ELF divergir do hash fixado, o serviço
  rejeita até atualizar o catálogo e recompilar o cliente com novo digest.
- Instalação automática apenas de **ELF único**; ZIP/7z complexos precisam de
  extração e validação específica, não incluídas nesta versão.
- Sem retomada de download nesta versão: falha ou cancelamento exclui o .PART.
- Um backup pendente é preservado para recuperação manual.
- Não é uma loja de jogos comerciais; respeite licenças e distribuição.
- Não modifica PadEmu em jogos nem os drivers existentes.
- Em hardware FAT, a instalação depende da implementação de `fileXioRename`.
