# TP-CLOUD 🌐 – Cliente e Servidor com IPv4/IPv6

Este é o trabalho prático da disciplina de Redes de Computadores, onde desenvolvemos uma aplicação cliente-servidor em C capaz de funcionar tanto com IPv4 quanto com IPv6. O projeto simula o envio de arquivos (como se fosse um serviço de armazenamento em nuvem), com direito a benchmark de velocidade, relatório explicando tudo e gráficos com os testes.

---

## 🛠 Sobre o que foi feito

O projeto implementa:

- Conexão TCP via sockets (com suporte dual: IPv4 e IPv6)
- Envio de diversos arquivos de um diretório (PDF, imagem, planilha, etc.)
- Protocolo de controle simples com handshake (ex: cliente envia "READY")
- Envio com medição de tempo para análises de desempenho
- Testes separados em redes IPv4 e IPv6 (com gráfico de comparação)
- Código limpo, bem comentado e dividido em cliente e servidor

---

## 📁 Estrutura do projeto

```
TP-CLOUD/
├── cliente.c                # Código-fonte do cliente
├── servidor.c              # Código-fonte do servidor
├── IagoReis-dados.txt      # Arquivo com nome para testes
├── TP–Cloud.pdf            # Relatório completo do trabalho
├── dados/                  # Arquivos usados para envio
│   ├── apresentacao.pptx
│   ├── documento.pdf
│   ├── planilha.xlsx
│   ├── foto.jpg
│   └── bye                 # Token de encerramento
```

---

## 🚀 Como compilar e rodar

### Compilar

No terminal (Linux ou WSL):

```bash
gcc servidor.c -o servidor
gcc cliente.c -o cliente
```

### Executar o servidor

```bash
./servidor
```

O servidor vai escutar na porta 8080 (IPv6 preferencialmente, mas aceita IPv4 também).

### Executar o cliente

```bash
./cliente <host> <porta> <diretório_de_arquivos>
```

Exemplo:

```bash
./cliente ::1 8080 dados/
```

---

## 📊 Resultados e Análises

No relatório (`TP–Cloud.pdf`) explicamos:

- Como o sistema foi feito
- Desempenho em IPv4 vs IPv6 (tempo médio, gráfico)
- Desafios encontrados
- Prints de testes reais
- Referências e justificativas técnicas

---

## 🎓 Créditos

Trabalho desenvolvido por **Iago Reis** para a disciplina de Redes de Computadores.

---

## 🔒 Observações

- Projeto em C puro, compatível com Linux.
- Utiliza funções clássicas de socket: `getaddrinfo`, `socket`, `connect`, `bind`, `listen`, `accept`, `send`, `recv`, etc.
- Organização pensada para facilitar leitura e testes.

---
