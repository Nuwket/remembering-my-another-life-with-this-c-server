/**
 * i18n.js — English, Portuguese and Russian copy for the lab.
 * Pure data plus language detection; no framework, no build step.
 */

export const I18N = {
  en: {
    navCore: "Overview", navStorage: "Storage", navSecurity: "Security",
    navHttp: "HTTP", navFailure: "Failure", navPerf: "Perf", navTech: "Tech",
    heroKicker: "Live backend · core 01",
    heroTag: "A hands-on laboratory for an HTTP server engineered in C. Every control hits the real process — no mocks, no sample data.",
    heroCta: "Open the lab", heroTech: "Tech mode",
    sysStatus: "System status", storageLabel: "Storage",
    chStorage: "Storage lab", chVault: "Security / vault",
    chFailure: "Failure lab", chPerf: "Performance lab",
    chLang: "Language lab", chTech: "Advanced",
    outLabel: "Output", lastCallLabel: "Last call", historyLabel: "History",
    howTag: "How it works", cmpTag: "Other languages",
    flowMsg: "Message", flowReq: "HTTP request", flowSrv: "C server", flowRes: "Response",
    busyWrite: "WRITING TO SQLITE…", busyReq: "EXECUTING REQUEST…", busyBench: "RUNNING BENCHMARK…",
    online: "online", offline: "unreachable", idle: "ready",
    sReq: "requests", sErr: "errors", sKv: "kv keys", sNotes: "notes", sConn: "connections",

    kvTitle: "Save a value", kvSub: "Key-value store, backed by SQLite",
    kvNote: "Give the server a name and a value. It writes the row and reports back what it did.",
    lKey: "key", lValue: "value",
    bSave: "Save", bFetch: "Fetch", bDelete: "Delete",
    confirmKv: "Delete the stored value",
    howSave: "How does it work?",
    cmpSave: "How would other languages do this?",

    noteTitle: "Notes", noteSub: "Numbered rows in the same database",
    noteNote: "Create a note, then edit or delete it from the list. Ids are assigned by the server.",
    lTitle: "title", lText: "text",
    bCreate: "Create note", bList: "Refresh list",
    editNote: "Edit", deleteNote: "Delete",
    confirmNote: "Delete note",
    howNote: "How does it work?",
    cmpNote: "How would other languages do this?",

    authTitle: "Vault", authSub: "Optional password for write operations",
    authNote: "With --api-key, writes require a matching X-API-Key header. Reads stay open.",
    modeOpen: "open", modeProtected: "protected",
    authOpenNotice: "This server runs open, so password tests answer 200. Restart with --api-key to see 401 and 403.",
    lPass: "demo password", defaultToken: "demo",
    bAuthGood: "Right password", bAuthWrong: "Wrong password", bAuthNone: "No password", bForget: "Forget",
    wrongKey: "definitely-wrong",
    typePassword: "Type a password first.",
    tokenForgotten: "Password forgotten.",
    howAuth: "How does it work?",
    cmpAuth: "How would other languages do this?",

    echoTitle: "Send a message", echoSub: "The server returns exactly what it received",
    echoNote: "Type anything. The value is JSON-encoded, sent, parsed back and shown again on the other side.",
    lMessage: "message", bSend: "Send",
    howEcho: "How does it work?",
    cmpEcho: "How would other languages do this?",

    errTitle: "Make the server complain", errSub: "Real rejections with real status codes",
    errNote: "Not every request has to succeed. Each button provokes a specific failure so you can watch validation and error mapping behave.",
    pGoneT: "Missing key", pGoneD: "Ask for a value that was never stored.",
    pIdT: "Invalid id", pIdD: "Note ids must be positive integers.",
    pMethodT: "Wrong method", pMethodD: "POST is not allowed on /health.",
    pDataT: "Invalid payload", pDataD: "Send a body without the required field.",
    pBigT: "Oversized body", pBigD: "Break the 64 KB request limit.",
    pRouteT: "Unknown route", pRouteD: "Ask for a path the server does not serve.",
    try: "Test",

    perfTitle: "How fast does it answer?", perfSub: "Real concurrent requests, timed in the browser",
    perfNote: "The runner fires N simultaneous GET /health calls and reports wall time plus the average per request. Nothing is simulated.",
    bRunPerf: "Run test", howPerf: "How does it work?",

    langTitle: "The same task, five languages",
    langSub: "Functionally equivalent, stylistically different",
    langNote: "Pick a language for the descriptive notes and to highlight its column. C snippets are real excerpts from this server; the others are labelled examples and are not executed here.",
    mPiece: "piece", implAria: "implementation language",

    drawerTitle: "Tech mode", drawerSub: "Inspector, history, curl and a raw composer",
    lPath: "path", lHeaders: "headers", lRawBody: "body",
    bExecute: "Execute", bCopyCurl: "Copy as curl", bClear: "Clear",
    mEndpoint: "endpoint", mTotal: "total",
    badHeader: "bad header line", nothingToCopy: "Nothing to copy yet.",
    copied: "Copied to clipboard", networkError: "network error",

    footNote: "Every button on this page calls the real C backend. No mocks.",

    realTag: "real code from this server", demoTag: "equivalent example, not executed here"
  },

  pt: {
    navCore: "Visão geral", navStorage: "Armazenamento", navSecurity: "Segurança",
    navHttp: "HTTP", navFailure: "Falhas", navPerf: "Perf", navTech: "Tech",
    heroKicker: "Backend ao vivo · núcleo 01",
    heroTag: "Um laboratório prático para um servidor HTTP escrito em C. Cada controle atinge o processo real — sem mocks, sem dados de exemplo.",
    heroCta: "Abrir o laboratório", heroTech: "Modo técnico",
    sysStatus: "Status do sistema", storageLabel: "Armazenamento",
    chStorage: "Laboratório de storage", chVault: "Segurança / cofre",
    chFailure: "Laboratório de falhas", chPerf: "Laboratório de performance",
    chLang: "Laboratório de linguagens", chTech: "Avançado",
    outLabel: "Saída", lastCallLabel: "Última chamada", historyLabel: "Histórico",
    howTag: "Como funciona", cmpTag: "Outras linguagens",
    flowMsg: "Mensagem", flowReq: "Requisição HTTP", flowSrv: "Servidor C", flowRes: "Resposta",
    busyWrite: "GRAVANDO NO SQLITE…", busyReq: "EXECUTANDO REQUISIÇÃO…", busyBench: "RODANDO BENCHMARK…",
    online: "online", offline: "sem conexão", idle: "pronto",
    sReq: "requisições", sErr: "erros", sKv: "chaves kv", sNotes: "notas", sConn: "conexões",

    kvTitle: "Guardar um valor", kvSub: "Armazenamento chave-valor, no SQLite",
    kvNote: "Dê um nome e um valor ao servidor. Ele grava a linha e informa o que fez.",
    lKey: "chave", lValue: "valor",
    bSave: "Salvar", bFetch: "Buscar", bDelete: "Apagar",
    confirmKv: "Apagar o valor guardado",
    howSave: "Como funciona?",
    cmpSave: "Como outras linguagens fariam isso?",

    noteTitle: "Notas", noteSub: "Linhas numeradas no mesmo banco",
    noteNote: "Crie uma nota e edite ou apague pela lista. Os ids são atribuídos pelo servidor.",
    lTitle: "título", lText: "texto",
    bCreate: "Criar nota", bList: "Atualizar lista",
    editNote: "Editar", deleteNote: "Apagar",
    confirmNote: "Apagar nota",
    howNote: "Como funciona?",
    cmpNote: "Como outras linguagens fariam isso?",

    authTitle: "Cofre", authSub: "Senha opcional para escritas",
    authNote: "Com --api-key, escritas exigem X-API-Key correspondente. Leituras continuam abertas.",
    modeOpen: "aberto", modeProtected: "protegido",
    authOpenNotice: "Este servidor está aberto, então os testes de senha respondem 200. Reinicie com --api-key para ver 401 e 403.",
    lPass: "senha de demonstração", defaultToken: "demo",
    bAuthGood: "Senha correta", bAuthWrong: "Senha errada", bAuthNone: "Sem senha", bForget: "Esquecer",
    wrongKey: "definitely-wrong",
    typePassword: "Escreva uma senha primeiro.",
    tokenForgotten: "Senha esquecida.",
    howAuth: "Como funciona?",
    cmpAuth: "Como outras linguagens fariam isso?",

    echoTitle: "Enviar mensagem", echoSub: "O servidor devolve exatamente o que recebeu",
    echoNote: "Escreva qualquer coisa. O valor vira JSON, é enviado, interpretado e mostrado do outro lado.",
    lMessage: "mensagem", bSend: "Enviar",
    howEcho: "Como funciona?",
    cmpEcho: "Como outras linguagens fariam isso?",

    errTitle: "Faça o servidor reclamar", errSub: "Rejeições reais com códigos reais",
    errNote: "Nem toda requisição precisa funcionar. Cada botão provoca uma falha específica para observar validação e mapeamento de erros.",
    pGoneT: "Chave ausente", pGoneD: "Peça um valor que nunca foi guardado.",
    pIdT: "Id inválido", pIdD: "Ids de nota devem ser inteiros positivos.",
    pMethodT: "Método errado", pMethodD: "POST não é permitido em /health.",
    pDataT: "Payload inválido", pDataD: "Envie um corpo sem o campo obrigatório.",
    pBigT: "Corpo grande", pBigD: "Estoure o limite de 64 KB.",
    pRouteT: "Rota desconhecida", pRouteD: "Peça um caminho que o servidor não serve.",
    try: "Testar",

    perfTitle: "Quão rápido ele responde?", perfSub: "Requisições reais e concorrentes, cronadas no navegador",
    perfNote: "O teste dispara N chamadas simultâneas a GET /health e informa o tempo total e a média. Nada é simulado.",
    bRunPerf: "Rodar teste", howPerf: "Como funciona?",

    langTitle: "A mesma tarefa, cinco linguagens", langSub: "Equivalentes no resultado, diferentes na forma",
    langNote: "Escolha uma linguagem para ver as notas e destacar a coluna. Os trechos em C são reais deste servidor; os outros são exemplos rotulados e não são executados aqui.",
    mPiece: "peça", implAria: "linguagem de implementação",

    drawerTitle: "Modo técnico", drawerSub: "Inspetor, histórico, curl e composer cru",
    lPath: "rota", lHeaders: "headers", lRawBody: "corpo",
    bExecute: "Executar", bCopyCurl: "Copiar como curl", bClear: "Limpar",
    mEndpoint: "endpoint", mTotal: "total",
    badHeader: "header inválido", nothingToCopy: "Nada para copiar ainda.",
    copied: "Copiado para a área de transferência", networkError: "erro de rede",

    footNote: "Todo botão desta página chama o backend C real. Sem mocks.",

    realTag: "código real deste servidor", demoTag: "exemplo equivalente, não executado aqui"
  },

  ru: {
    navCore: "Обзор", navStorage: "Хранилище", navSecurity: "Безопасность",
    navHttp: "HTTP", navFailure: "Отказы", navPerf: "Перф", navTech: "Tech",
    heroKicker: "Живой бэкенд · ядро 01",
    heroTag: "Практическая лаборатория для HTTP-сервера на C. Каждая кнопка бьёт в реальный процесс — без моков и демо-данных.",
    heroCta: "Открыть лабораторию", heroTech: "Техрежим",
    sysStatus: "Состояние системы", storageLabel: "Хранилище",
    chStorage: "Лаборатория хранилища", chVault: "Безопасность / сейф",
    chFailure: "Лаборатория отказов", chPerf: "Лаборатория производительности",
    chLang: "Лаборатория языков", chTech: "Расширенное",
    outLabel: "Вывод", lastCallLabel: "Последний вызов", historyLabel: "История",
    howTag: "Как это работает", cmpTag: "Другие языки",
    flowMsg: "Сообщение", flowReq: "HTTP-запрос", flowSrv: "C-сервер", flowRes: "Ответ",
    busyWrite: "ЗАПИСЬ В SQLITE…", busyReq: "ВЫПОЛНЕНИЕ ЗАПРОСА…", busyBench: "ЗАМЕР БЕНЧМАРКА…",
    online: "в сети", offline: "нет связи", idle: "готово",
    sReq: "запросы", sErr: "ошибки", sKv: "ключи kv", sNotes: "заметки", sConn: "соединения",

    kvTitle: "Сохранить значение", kvSub: "Хранилище ключ-значение на SQLite",
    kvNote: "Дайте серверу имя и значение. Он запишет строку и сообщит результат.",
    lKey: "ключ", lValue: "значение",
    bSave: "Сохранить", bFetch: "Найти", bDelete: "Удалить",
    confirmKv: "Удалить сохранённое значение",
    howSave: "Как это работает?",
    cmpSave: "Как это сделали бы другие языки?",

    noteTitle: "Заметки", noteSub: "Нумерованные строки в той же базе",
    noteNote: "Создайте заметку и отредактируйте или удалите её из списка. Id назначает сервер.",
    lTitle: "заголовок", lText: "текст",
    bCreate: "Создать заметку", bList: "Обновить список",
    editNote: "Изменить", deleteNote: "Удалить",
    confirmNote: "Удалить заметку",
    howNote: "Как это работает?",
    cmpNote: "Как это сделали бы другие языки?",

    authTitle: "Сейф", authSub: "Необязательный пароль для записи",
    authNote: "С --api-key запись требует заголовок X-API-Key. Чтение остаётся открытым.",
    modeOpen: "открыт", modeProtected: "защищён",
    authOpenNotice: "Сервер открыт, поэтому тесты пароля вернут 200. Перезапустите с --api-key, чтобы увидеть 401 и 403.",
    lPass: "демо-пароль", defaultToken: "demo",
    bAuthGood: "Верный пароль", bAuthWrong: "Неверный пароль", bAuthNone: "Без пароля", bForget: "Забыть",
    wrongKey: "definitely-wrong",
    typePassword: "Сначала введите пароль.",
    tokenForgotten: "Пароль забыт.",
    howAuth: "Как это работает?",
    cmpAuth: "Как это сделали бы другие языки?",

    echoTitle: "Отправить сообщение", echoSub: "Сервер возвращает ровно то, что получил",
    echoNote: "Напишите что угодно. Значение станет JSON, уйдёт, разберётся и вернётся обратно.",
    lMessage: "сообщение", bSend: "Отправить",
    howEcho: "Как это работает?",
    cmpEcho: "Как это сделали бы другие языки?",

    errTitle: "Заставьте сервер жаловаться", errSub: "Настоящие отказы с настоящими кодами",
    errNote: "Не каждый запрос обязан работать. Каждая кнопка вызывает конкретный сбой, чтобы увидеть валидацию и маппинг ошибок.",
    pGoneT: "Нет ключа", pGoneD: "Попросите значение, которого никто не сохранял.",
    pIdT: "Неверный id", pIdD: "Id заметки должен быть положительным целым.",
    pMethodT: "Не тот метод", pMethodD: "POST не разрешён на /health.",
    pDataT: "Битый payload", pDataD: "Отправьте тело без обязательного поля.",
    pBigT: "Слишком большой", pBigD: "Превысьте лимит 64 KB.",
    pRouteT: "Нет роута", pRouteD: "Запросите путь, которого сервер не обслуживает.",
    try: "Проверить",

    perfTitle: "Как быстро он отвечает?", perfSub: "Настоящие параллельные запросы, замер в браузере",
    perfNote: "Тест отправляет N одновременных вызовов GET /health и сообщает общее время и среднее. Ничего не подделано.",
    bRunPerf: "Запустить тест", howPerf: "Как это работает?",

    langTitle: "Одна задача — пять языков", langSub: "Одинаковый результат, разная форма",
    langNote: "Выберите язык для описаний и подсветки колонки. Фрагменты на C настоящие; остальные — помеченные примеры, которые здесь не выполняются.",
    mPiece: "часть", implAria: "язык реализации",

    drawerTitle: "Техрежим", drawerSub: "Инспектор, история, curl и прямой composer",
    lPath: "путь", lHeaders: "заголовки", lRawBody: "тело",
    bExecute: "Выполнить", bCopyCurl: "Копировать как curl", bClear: "Очистить",
    mEndpoint: "эндпоинт", mTotal: "всего",
    badHeader: "плохой заголовок", nothingToCopy: "Пока нечего копировать.",
    copied: "Скопировано в буфер обмена", networkError: "сетевая ошибка",

    footNote: "Каждая кнопка обращается к настоящему C-бэкенду. Без моков.",

    realTag: "настоящий код этого сервера", demoTag: "аналог, здесь не выполняется"
  }
};

/** Human verdicts shown after each action. */
export const MSGS = {
  en: {
    saved: (k, v) => "Saved: " + k + " = " + v,
    fetched: (v) => "The server answered: " + v,
    deleted: () => "The server confirmed the value is gone.",
    notFound: () => "Nothing is stored under that key or id.",
    badRequest: () => "The server rejected the input.",
    methodNotAllowed: () => "That operation is not allowed here.",
    tooLarge: () => "The body is over the 64 KB limit.",
    internalError: () => "The server reported an internal failure.",
    noteCreated: (id) => "Note created with id " + id + ".",
    noteUpdated: (id) => "Note " + id + " updated.",
    noteDeleted: (id) => "Note " + id + " deleted.",
    notesListed: (n) => "The server returned " + n + " note(s).",
    authAccepted: () => "Password accepted. The write went through.",
    authRejected: () => "Refused: the password does not match.",
    authMissing: () => "Refused: no password was sent.",
    authOpenServer: () => "Allowed: this server is running open, so no key is required.",
    echoRoundTrip: (sent, got) => "You sent: \"" + sent + "\"\nThe server answered: \"" + got + "\"",
    perf: (n, errs, total, avg) => n + " real requests · " + errs + " errors · " + total + "ms total · " + avg + "ms average"
  },
  pt: {
    saved: (k, v) => "Guardado: " + k + " = " + v,
    fetched: (v) => "O servidor respondeu: " + v,
    deleted: () => "O servidor confirmou que o valor não existe mais.",
    notFound: () => "Nada guardado com essa chave ou id.",
    badRequest: () => "O servidor rejeitou a entrada.",
    methodNotAllowed: () => "Essa operação não é permitida aqui.",
    tooLarge: () => "O corpo passou do limite de 64 KB.",
    internalError: () => "O servidor relatou uma falha interna.",
    noteCreated: (id) => "Nota criada com id " + id + ".",
    noteUpdated: (id) => "Nota " + id + " atualizada.",
    noteDeleted: (id) => "Nota " + id + " apagada.",
    notesListed: (n) => "O servidor devolveu " + n + " nota(s).",
    authAccepted: () => "Senha aceita. A escrita passou.",
    authRejected: () => "Recusado: a senha não confere.",
    authMissing: () => "Recusado: nenhuma senha foi enviada.",
    authOpenServer: () => "Permitido: este servidor está aberto, então não exige chave.",
    echoRoundTrip: (sent, got) => "Você enviou: \"" + sent + "\"\nO servidor respondeu: \"" + got + "\"",
    perf: (n, errs, total, avg) => n + " requisições reais · " + errs + " erros · " + total + "ms no total · " + avg + "ms em média"
  },
  ru: {
    saved: (k, v) => "Сохранено: " + k + " = " + v,
    fetched: (v) => "Сервер ответил: " + v,
    deleted: () => "Сервер подтвердил, что значения больше нет.",
    notFound: () => "Под таким ключом или id ничего нет.",
    badRequest: () => "Сервер отклонил входные данные.",
    methodNotAllowed: () => "Такая операция здесь запрещена.",
    tooLarge: () => "Тело больше лимита 64 KB.",
    internalError: () => "Сервер сообщил о внутренней ошибке.",
    noteCreated: (id) => "Заметка создана с id " + id + ".",
    noteUpdated: (id) => "Заметка " + id + " обновлена.",
    noteDeleted: (id) => "Заметка " + id + " удалена.",
    notesListed: (n) => "Сервер вернул заметок: " + n + ".",
    authAccepted: () => "Пароль принят. Запись прошла.",
    authRejected: () => "Отказано: пароль не совпадает.",
    authMissing: () => "Отказано: пароль не отправлен.",
    authOpenServer: () => "Разрешено: сервер открыт и не требует ключ.",
    echoRoundTrip: (sent, got) => "Вы отправили: \"" + sent + "\"\nСервер ответил: \"" + got + "\"",
    perf: (n, errs, total, avg) => n + " настоящих запросов · ошибок: " + errs + " · " + total + "мс всего · " + avg + "мс в среднем"
  }
};

/** Step-by-step explainers shown inside the disclosures. */
export const HOW = {
  en: {
    save: "<ol><li>You pressed <b>Save</b>.</li><li>The browser sent the name and value as JSON.</li><li>The C server bound the parameters in a prepared statement and wrote the row to SQLite.</li><li>The browser rendered the server's answer in plain language.</li></ol>Technically: <code>PUT /api/kv/:key &rarr; 200</code>",
    note: "<ol><li>You pressed <b>Create note</b>.</li><li>The title and text travelled as a JSON body.</li><li>SQLite assigned the next auto-increment id and stored the row.</li><li>The created note came back and is now listed below.</li></ol>Technically: <code>POST /api/notes &rarr; 201</code>",
    auth: "<ol><li>You picked a password scenario.</li><li>The browser attached it as an <code>X-API-Key</code> header, or deliberately did not.</li><li>The server compared it in constant time and allowed or refused.</li><li>Open servers accept everything, which is why the badge says so.</li></ol>Technically: <code>PUT /api/kv/... &rarr; 200 / 401 / 403</code>",
    echo: "<ol><li>You pressed <b>Send</b>.</li><li>Your text was JSON-encoded into a request body.</li><li>The server parsed the body, validated the <code>data</code> field and echoed it back.</li><li>Both sides are shown so the round trip is visible.</li></ol>Technically: <code>POST /api/echo &rarr; 200</code>",
    perf: "<ol><li>You chose a request count and pressed <b>Run test</b>.</li><li>The browser fired that many concurrent <code>GET /health</code> calls.</li><li>It measured wall time, counted failures and computed the average.</li></ol>Timings include your local network, so they are indicative rather than a benchmark."
  },
  pt: {
    save: "<ol><li>Você clicou em <b>Salvar</b>.</li><li>O navegador enviou nome e valor como JSON.</li><li>O servidor C vinculou os parâmetros num prepared statement e gravou a linha no SQLite.</li><li>O navegador mostrou a resposta em linguagem simples.</li></ol>Tecnicamente: <code>PUT /api/kv/:key &rarr; 200</code>",
    note: "<ol><li>Você clicou em <b>Criar nota</b>.</li><li>Título e texto foram como corpo JSON.</li><li>O SQLite atribuiu o próximo id automático e guardou a linha.</li><li>A nota criada voltou e já aparece na lista.</li></ol>Tecnicamente: <code>POST /api/notes &rarr; 201</code>",
    auth: "<ol><li>Você escolheu um cenário de senha.</li><li>O navegador anexou como header <code>X-API-Key</code>, ou não anexou de propósito.</li><li>O servidor comparou em tempo constante e permitiu ou recusou.</li><li>Servidores abertos aceitam tudo, e o badge avisa isso.</li></ol>Tecnicamente: <code>PUT /api/kv/... &rarr; 200 / 401 / 403</code>",
    echo: "<ol><li>Você clicou em <b>Enviar</b>.</li><li>Seu texto virou um corpo JSON.</li><li>O servidor interpretou, validou o campo <code>data</code> e devolveu.</li><li>Os dois lados aparecem para a ida e volta ficar visível.</li></ol>Tecnicamente: <code>POST /api/echo &rarr; 200</code>",
    perf: "<ol><li>Você escolheu uma quantidade e clicou em <b>Rodar teste</b>.</li><li>O navegador disparou essa mesma quantidade de chamadas simultâneas a <code>GET /health</code>.</li><li>Mediu o tempo total, contou falhas e calculou a média.</li></ol>Os tempos incluem sua rede local, então são indicativos, não um benchmark."
  },
  ru: {
    save: "<ol><li>Вы нажали <b>Сохранить</b>.</li><li>Браузер отправил имя и значение как JSON.</li><li>C-сервер связал параметры в prepared statement и записал строку в SQLite.</li><li>Браузер показал ответ простым языком.</li></ol>Технически: <code>PUT /api/kv/:key &rarr; 200</code>",
    note: "<ol><li>Вы нажали <b>Создать заметку</b>.</li><li>Заголовок и текст ушли телом JSON.</li><li>SQLite выдал следующий авто-id и сохранил строку.</li><li>Созданная заметка вернулась и появилась в списке.</li></ol>Технически: <code>POST /api/notes &rarr; 201</code>",
    auth: "<ol><li>Вы выбрали сценарий с паролем.</li><li>Браузер добавил его как заголовок <code>X-API-Key</code> или намеренно не добавил.</li><li>Сервер сравнил за константное время и разрешил или отказал.</li><li>Открытый сервер принимает всё, об этом сообщает бейдж.</li></ol>Технически: <code>PUT /api/kv/... &rarr; 200 / 401 / 403</code>",
    echo: "<ol><li>Вы нажали <b>Отправить</b>.</li><li>Текст стал телом JSON.</li><li>Сервер разобрал его, проверил поле <code>data</code> и вернул обратно.</li><li>Обе стороны показаны, чтобы круговой обмен был виден.</li></ol>Технически: <code>POST /api/echo &rarr; 200</code>",
    perf: "<ol><li>Вы выбрали количество и нажали <b>Запустить тест</b>.</li><li>Браузер отправил столько одновременных вызовов <code>GET /health</code>.</li><li>Измерил общее время, посчитал сбои и вычислил среднее.</li></ol>Время включает вашу локальную сеть, поэтому это ориентир, а не бенчмарк."
  }
};

/** Field tooltips, native `title` so they work without extra JS. */
export const TIPS = {
  en: {
    key: "Letters, digits, dot, underscore and dash. 1-128 characters.",
    value: "Stored as a JSON string, up to about 60 KB.",
    title: "Required, 1-200 characters.",
    body: "Optional note text, up to 8192 characters.",
    tok: "Sent as an X-API-Key header on writes. Kept only in this browser.",
    path: "Must start with a slash. Try /health, /metrics, /api/kv/demo",
    hdrs: "One 'Name: value' header per line.",
    rawbody: "Sent byte for byte. Invalid JSON is a valid test."
  },
  pt: {
    key: "Letras, dígitos, ponto, underscore e traço. 1-128 caracteres.",
    value: "Guardado como string JSON, até cerca de 60 KB.",
    title: "Obrigatório, 1-200 caracteres.",
    body: "Texto opcional da nota, até 8192 caracteres.",
    tok: "Enviado como header X-API-Key nas escritas. Fica só neste navegador.",
    path: "Precisa começar com /. Tente /health, /metrics, /api/kv/demo",
    hdrs: "Um header 'Nome: valor' por linha.",
    rawbody: "Enviado byte a byte. JSON inválido é um teste válido."
  },
  ru: {
    key: "Буквы, цифры, точка, подчёркивание и дефис. 1-128 символов.",
    value: "Хранится как строка JSON, до ~60 KB.",
    title: "Обязательно, 1-200 символов.",
    body: "Необязательный текст заметки, до 8192 символов.",
    tok: "Шлётся заголовком X-API-Key при записи. Только в этом браузере.",
    path: "Должен начинаться с /. Попробуйте /health, /metrics, /api/kv/demo",
    hdrs: "Один заголовок 'Имя: значение' в строке.",
    rawbody: "Шлётся байт в байт. Битый JSON — валидный тест."
  }
};

export function detectLang() {
  try {
    const saved = localStorage.getItem("capi-lang");
    if (saved && I18N[saved]) return saved;
  } catch (e) { /* private mode */ }
  const nav = (navigator.language || "en").toLowerCase();
  if (nav.indexOf("ru") === 0) return "ru";
  if (nav.indexOf("pt") === 0) return "pt";
  return "en";
}
