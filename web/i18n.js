/**
 * i18n.js - Trilingual dictionaries (EN/PT/RU) for the lab.
 * Plain data + lookup helpers. No framework, no build step.
 */
export const LANGS = ["en", "pt", "ru"];

export const I18N = {
  en: {
    langLabel: "language", labWord: "Lab",
    heroSub: "Try the server's features. You don't need to know APIs.",
    probing: "probing /health…", mReq: "requests", mErr: "errors", mKv: "kv keys", mNotes: "notes", mConn: "connections",
    saveTitle: "Save a value", saveHint: "The server can store information. Give it a name and a value, then save.",
    nameL: "name", valL: "value", saveBtn: "🟢 SAVE", fetchBtn: "📦 FETCH IT", delBtn: "🗑️ DELETE IT",
    notesTitle: "Notes", notesHint: "The server also keeps small numbered notes.",
    titleL: "title", bodyL: "text", createNote: "🟢 CREATE NOTE", listNotes: "📋 SEE ALL NOTES",
    authTitle: "Vault", authHint: "The server may require a password to allow changes. Let's test.",
    passL: "demo password", authGood: "🟢 ENTER WITH THE RIGHT PASSWORD", authBad: "🔴 TRY THE WRONG PASSWORD",
    authNone: "⚪ TRY WITH NO PASSWORD",
    openNote: "Server is OPEN (no --api-key): password demos will return 200. Restart with an API key to see real 401/403.",
    echoTitle: "Send a message", echoHint: "The server sends back exactly what it receives. Write something and send it.",
    msgL: "message", sendBtn: "🟢 SEND",
    errTitle: "Make the server complain", errHint: "Not every request has to work. Click to see how the server handles problems.",
    eGoneT: "🟡 Fetch something missing", eGoneD: "Ask for a value that was never saved.",
    eAuthT: "🟡 Send a wrong password", eAuthD: "See what happens when authentication fails.",
    eDataT: "🟡 Send invalid data", eDataD: "Send something the server cannot accept.",
    eOpT: "🟡 Use the wrong operation", eOpD: "Try something this function does not allow.",
    eBigT: "🟡 Send a giant value", eBigD: "Break the 64KB limit on purpose.",
    eRouteT: "🟡 Ask an unknown address", eRouteD: "Call a route the server never heard of.",
    tryBtn: "TEST",
    perfTitle: "How fast does the server answer?", perfHint: "The test sends real requests and times them. Nothing is faked.",
    runTest: "🟢 RUN TEST", linksTitle: "Shortcuts",
    cmpQ: "🔎 How would other languages do this?", cmpQget: "🔎 …and to FETCH?", cmpQdel: "🔎 …and to DELETE?",
    langTitle: "The same task, five languages",
    langHint: "Everyone solves the same problem. What changes is how each language expresses it — and what it asks of you. No verdict: read and compare.",
    colPiece: "piece", cmpAria: "implementation language", realTag: "real code from this server", demoTag: "equivalent example — NOT executed here",
    copied: "copied",
    techTitle: "Tech mode — see what happened underneath",
    techHint: "Method, URL, headers, body, status, response, time and curl for every call.",
    forget: "Forget", pathL: "path",
    headersL: "headers (one per line)", bodyL2: "body (raw)", execute: "Execute", clear: "Clear",
    colRoute: "endpoint", colTotal: "total",
    howQ: "? How does it work?", tipsTitle: "Field tips",
    modeOpen: "open — no password configured",
    modeProtected: "protected — password required",
    confirmDeleteKv: "Delete the saved value",
    confirmDeleteNote: "Delete note",
    typePassword: "Type a password first.",
    tokenForgotten: "Password forgotten.",
    wrongKey: "definitely-wrong",
    editNote: "Edit",
    deleteNote: "Delete",
    unreachable: "unreachable",
    nothingToCopy: "Nothing to copy yet.",
    footer: "C API Server — everything here hits the real C backend. No mocks."
  },
  pt: {
    langLabel: "idioma", labWord: "Laboratório",
    heroSub: "Experimente as funções do servidor. Você não precisa saber API.",
    probing: "consultando /health…", mReq: "requisições", mErr: "erros", mKv: "chaves kv", mNotes: "notas", mConn: "conexões",
    saveTitle: "Guardar um dado", saveHint: "O servidor consegue guardar informações. Dê um nome e um valor, e guarde.",
    nameL: "Nome", valL: "Valor", saveBtn: "🟢 GUARDAR", fetchBtn: "📦 BUSCAR ESSE DADO", delBtn: "🗑️ APAGAR ESSE DADO",
    notesTitle: "Notas", notesHint: "O servidor também guarda pequenas notas numeradas.",
    titleL: "título", bodyL: "texto", createNote: "🟢 CRIAR NOTA", listNotes: "📋 VER TODAS AS NOTAS",
    authTitle: "Cofre", authHint: "O servidor pode exigir uma senha para permitir alterações. Vamos testar.",
    passL: "senha de demonstração", authGood: "🟢 ENTRAR COM SENHA CORRETA", authBad: "🔴 TENTAR COM SENHA ERRADA",
    authNone: "⚪ TENTAR SEM SENHA",
    openNote: "Servidor ABERTO (sem --api-key): demos de senha vão retornar 200. Reinicie com API key para ver 401/403 de verdade.",
    echoTitle: "Mandar uma mensagem", echoHint: "O servidor devolve exatamente o que recebeu. Escreva algo e envie.",
    msgL: "mensagem", sendBtn: "🟢 ENVIAR",
    errTitle: "Faça o servidor reclamar", errHint: "Nem toda requisição precisa funcionar. Clique para ver como o servidor lida com problemas.",
    eGoneT: "🟡 Buscar algo que não existe", eGoneD: "Peça um dado que nunca foi guardado.",
    eAuthT: "🟡 Mandar uma senha errada", eAuthD: "Veja o que acontece quando a autenticação falha.",
    eDataT: "🟡 Enviar dados inválidos", eDataD: "Envie algo que o servidor não consegue aceitar.",
    eOpT: "🟡 Usar a operação errada", eOpD: "Tente fazer algo que essa função não permite.",
    eBigT: "🟡 Mandar dado gigante", eBigD: "Ultrapasse o limite de 64KB de propósito.",
    eRouteT: "🟡 Pedir rota inexistente", eRouteD: "Chame um endereço que o servidor não conhece.",
    tryBtn: "TESTAR",
    perfTitle: "Quantas vezes o servidor responde?", perfHint: "O teste envia requisições reais e mede o tempo. Nada é simulado.",
    runTest: "🟢 RODAR TESTE", linksTitle: "Atalhos",
    cmpQ: "🔎 Como outras linguagens fariam isso?", cmpQget: "🔎 …e para BUSCAR?", cmpQdel: "🔎 …e para APAGAR?",
    langTitle: "A mesma tarefa, cinco linguagens",
    langHint: "Todos resolvem o mesmo problema. O que muda é como cada linguagem expressa isso — e o que ela exige de você. Sem veredito: leia e compare.",
    colPiece: "peça", cmpAria: "linguagem de implementação", realTag: "código real deste servidor", demoTag: "exemplo equivalente — NÃO executado aqui",
    copied: "copiado",
    techTitle: "Modo técnico — ver o que aconteceu por baixo",
    techHint: "Método, URL, headers, corpo, status, resposta, tempo e cURL de cada chamada.",
    forget: "Esquecer", pathL: "rota",
    headersL: "headers (um por linha)", bodyL2: "corpo (bruto)", execute: "Executar", clear: "Limpar",
    colRoute: "endpoint", colTotal: "total",
    howQ: "? Como isso funciona?", tipsTitle: "Dicas dos campos",
    modeOpen: "aberto — sem senha configurada",
    modeProtected: "protegido — senha obrigatória",
    confirmDeleteKv: "Apagar o dado guardado",
    confirmDeleteNote: "Apagar nota",
    typePassword: "Escreva uma senha primeiro.",
    tokenForgotten: "Senha esquecida.",
    wrongKey: "definitely-wrong",
    editNote: "Editar",
    deleteNote: "Apagar",
    unreachable: "sem conexão",
    nothingToCopy: "Nada para copiar ainda.",
    footer: "C API Server — tudo aqui bate no backend C de verdade. Sem mocks."
  },
  ru: {
    langLabel: "язык", labWord: "Лаборатория",
    heroSub: "Попробуйте функции сервера. Знать API не нужно.",
    probing: "опрос /health…", mReq: "запросы", mErr: "ошибки", mKv: "ключи kv", mNotes: "заметки", mConn: "соединения",
    saveTitle: "Сохранить данное", saveHint: "Сервер умеет хранить информацию. Дайте имя и значение — и сохраните.",
    nameL: "Имя", valL: "Значение", saveBtn: "🟢 СОХРАНИТЬ", fetchBtn: "📦 НАЙТИ ЕГО", delBtn: "🗑️ УДАЛИТЬ ЕГО",
    notesTitle: "Заметки", notesHint: "Сервер также хранит маленькие номерные заметки.",
    titleL: "заголовок", bodyL: "текст", createNote: "🟢 СОЗДАТЬ ЗАМЕТКУ", listNotes: "📋 ВСЕ ЗАМЕТКИ",
    authTitle: "Сейф", authHint: "Сервер может требовать пароль для изменений. Давайте проверим.",
    passL: "демо-пароль", authGood: "🟢 ВОЙТИ С ВЕРНЫМ ПАРОЛЕМ", authBad: "🔴 НЕВЕРНЫЙ ПАРОЛЬ",
    authNone: "⚪ БЕЗ ПАРОЛЯ",
    openNote: "Сервер ОТКРЫТ (без --api-key): демо вернут 200. Перезапустите с ключом, чтобы увидеть настоящие 401/403.",
    echoTitle: "Отправить сообщение", echoHint: "Сервер возвращает ровно то, что получил. Напишите что-нибудь.",
    msgL: "сообщение", sendBtn: "🟢 ОТПРАВИТЬ",
    errTitle: "Заставьте сервер жаловаться", errHint: "Не каждый запрос обязан работать. Нажмите и смотрите, как сервер решает проблемы.",
    eGoneT: "🟡 Найти несуществующее", eGoneD: "Попросите данное, которое никто не сохранял.",
    eAuthT: "🟡 Неверный пароль", eAuthD: "Смотрите, что бывает при ошибке входа.",
    eDataT: "🟡 Битые данные", eDataD: "Отправьте то, что сервер принять не может.",
    eOpT: "🟡 Не та операция", eOpD: "Попробуйте то, что функция не разрешает.",
    eBigT: "🟡 Гигантное значение", eBigD: "Специально превысьте лимит 64KB.",
    eRouteT: "🟡 Неизвестный адрес", eRouteD: "Вызовите роут, о котором сервер не слышал.",
    tryBtn: "ПРОБА",
    perfTitle: "Как быстро отвечает сервер?", perfHint: "Тест шлёт настоящие запросы и меряет время. Ничего не подделано.",
    runTest: "🟢 ЗАПУСТИТЬ ТЕСТ", linksTitle: "Ссылки",
    cmpQ: "🔎 А как бы это сделали другие языки?", cmpQget: "🔎 …а НАЙТИ?", cmpQdel: "🔎 …а УДАЛИТЬ?",
    langTitle: "Одна задача — пять языков",
    langHint: "Все решают одну задачу. Разница — в том, как каждый язык это выражает и что требует от вас. Без вердикта: читайте и сравнивайте.",
    colPiece: "часть", cmpAria: "язык реализации", realTag: "настоящий код этого сервера", demoTag: "аналог — здесь НЕ выполняется",
    copied: "скопировано",
    techTitle: "Техрежим — что было под капотом",
    techHint: "Метод, URL, заголовки, тело, статус, ответ, время и cURL каждого вызова.",
    forget: "Забыть", pathL: "путь",
    headersL: "заголовки (по одному в строке)", bodyL2: "тело (сырое)", execute: "Выполнить", clear: "Очистить",
    colRoute: "эндпоинт", colTotal: "total", 
    howQ: "? Как это работает?", tipsTitle: "Подсказки полей",
    modeOpen: "открыт — пароль не настроен",
    modeProtected: "защищён — пароль обязателен",
    confirmDeleteKv: "Удалить сохранённое значение",
    confirmDeleteNote: "Удалить заметку",
    typePassword: "Сначала введите пароль.",
    tokenForgotten: "Пароль забыт.",
    wrongKey: "definitely-wrong",
    editNote: "Изменить",
    deleteNote: "Удалить",
    unreachable: "нет связи",
    nothingToCopy: "Пока нечего копировать.",
    footer: "C API Server — всё здесь бьёт в настоящий C-бэкенд. Без моков."
  }
};

export const TIPS = {
  en: {
    key: "Letters, digits, dot, underscore, dash. 1–128 chars.",
    value: "Stored as a JSON string, up to ~60KB.",
    title: "Required, 1–200 chars.",
    body: "Optional text, up to 8192 chars.",
    tok: "Sent as X-API-Key header on writes. Lives only in this browser.",
    path: "Must start with /.",
    hdrs: "One 'Name: value' header per line.",
    rawbody: "Sent byte-for-byte."
  },
  pt: {
    key: "Letras, dígitos, ponto, underscore, traço. 1–128 caracteres.",
    value: "Guardado como string JSON, até ~60KB.",
    title: "Obrigatório, 1–200 caracteres.",
    body: "Texto opcional, até 8192 caracteres.",
    tok: "Enviado como header X-API-Key nas escritas. Fica só neste navegador.",
    path: "Deve começar com /.",
    hdrs: "Um header 'Nome: valor' por linha.",
    rawbody: "Enviado byte a byte."
  },
  ru: {
    key: "Буквы, цифры, точка, подчёркивание, дефис. 1–128 символов.",
    value: "Хранится как строка JSON, до ~60KB.",
    title: "Обязательно, 1–200 символов.",
    body: "Необязательный текст, до 8192 символов.",
    tok: "Шлётся заголовком X-API-Key при записи. Только в этом браузере.",
    path: "Должен начинаться с /.",
    hdrs: "Один заголовок 'Имя: значение' в строке.",
    rawbody: "Шлётся как есть."
  }
};

/** Human verdicts used by the lab sections. */
export const MSGS = {
  en: {
    saved: (k, v) => "✅ The server saved it: " + k + " = " + v,
    fetched: (v) => "📦 The server answered: " + v,
    deleted: () => "🗑️ The server confirmed the value was removed.",
    noteMade: (id) => "✅ Note saved with id " + id + ".",
    noteUpdated: (id) => "✅ Note " + id + " updated.",
    notesListed: (n) => "📋 The server returned " + n + " note(s) below.",
    noteGone: (id) => "🗑️ Note " + id + " deleted.",
    authOk: () => "✅ Password accepted. The server allowed the change.",
    authOpen: () => "✅ The server allowed it — it runs OPEN (no password configured).",
    authBad: () => "❌ The server refused: wrong password. (HTTP 403)",
    authNone: () => "🔒 The server refused: no password was sent. (HTTP 401)",
    echoGot: (a, b) => "📤 You sent: \"" + a + "\"\n📥 The server answered: \"" + b + "\"",
    err404: () => "❌ The server said: “not found”. (HTTP 404)",
    err403: () => "🔒 Access denied: wrong password. (HTTP 403)",
    err401: () => "🔒 Access denied: no password sent. (HTTP 401)",
    err400: () => "⚠️ The server rejected the data. (HTTP 400)",
    err405: () => "🚫 Operation not allowed here. (HTTP 405)",
    err413: () => "⚠️ Too big: over the 64KB limit. (HTTP 413)",
    perf: (n, e, t, a) => n + " real requests · " + e + " errors · " + t + "ms total · " + a + "ms average"
  },
  pt: {
    saved: (k, v) => "✅ O servidor guardou: " + k + " = " + v,
    fetched: (v) => "📦 O servidor respondeu: " + v,
    deleted: () => "🗑️ O servidor confirmou que o dado foi removido.",
    noteMade: (id) => "✅ Nota guardada com id " + id + ".",
    noteUpdated: (id) => "✅ Nota " + id + " atualizada.",
    notesListed: (n) => "📋 O servidor devolveu " + n + " nota(s) abaixo.",
    noteGone: (id) => "🗑️ Nota " + id + " apagada.",
    authOk: () => "✅ A senha foi aceita. O servidor permettrait a alteração.",
    authOpen: () => "✅ O servidor permitiu — ele está ABERTO (sem senha configurada).",
    authBad: () => "❌ O servidor recusou: senha errada. (HTTP 403)",
    authNone: () => "🔒 O servidor recusou: nenhuma senha enviada. (HTTP 401)",
    echoGot: (a, b) => "📤 Você enviou: \"" + a + "\"\n📥 O servidor respondeu: \"" + b + "\"",
    err404: () => "❌ O servidor disse: “não encontrado”. (HTTP 404)",
    err403: () => "🔒 Acesso recusado: senha errada. (HTTP 403)",
    err401: () => "🔒 Acesso recusado: sem senha. (HTTP 401)",
    err400: () => "⚠️ O servidor rejeitou os dados. (HTTP 400)",
    err405: () => "🚫 Operação não permitida aqui. (HTTP 405)",
    err413: () => "⚠️ Grande demais: passou de 64KB. (HTTP 413)",
    perf: (n, e, t, a) => n + " requisições reais · " + e + " erros · " + t + "ms total · " + a + "ms média"
  },
  ru: {
    saved: (k, v) => "✅ Сервер сохранил: " + k + " = " + v,
    fetched: (v) => "📦 Сервер ответил: " + v,
    deleted: () => "🗑️ Сервер подтвердил удаление.",
    noteMade: (id) => "✅ Заметка сохранена с id " + id + ".",
    noteUpdated: (id) => "✅ Заметка " + id + " обновлена.",
    notesListed: (n) => "📋 Сервер вернул заметок: " + n + ".",
    noteGone: (id) => "🗑️ Заметка " + id + " удалена.",
    authOk: () => "✅ Пароль принят. Сервер разрешил изменение.",
    authOpen: () => "✅ Сервер разрешил — он ОТКРЫТ (пароль не настроен).",
    authBad: () => "❌ Сервер отказал: неверный пароль. (HTTP 403)",
    authNone: () => "🔒 Сервер отказал: пароль не отправлен. (HTTP 401)",
    echoGot: (a, b) => "📤 Вы отправили: \"" + a + "\"\n📥 Сервер ответил: \"" + b + "\"",
    err404: () => "❌ Сервер сказал: «не найдено». (HTTP 404)",
    err403: () => "🔒 Доступ запрещён: неверный пароль. (HTTP 403)",
    err401: () => "🔒 Доступ запрещён: без пароля. (HTTP 401)",
    err400: () => "⚠️ Сервер отклонил данные. (HTTP 400)",
    err405: () => "🚫 Здесь так нельзя. (HTTP 405)",
    err413: () => "⚠️ Слишком много: больше 64KB. (HTTP 413)",
    perf: (n, e, t, a) => n + " настоящих запросов · ошибок: " + e + " · " + t + "мс всего · " + a + "мс в среднем"
  }
};

/** "How does it work?" bodies, keyed by section. */
export const HOW = {
  en: {
    save: "<ol><li>You clicked <b>SAVE</b>.</li><li>The browser sent the name and value to the C server.</li><li>The server wrote them into SQLite.</li><li>The browser showed you the answer.</li></ol>Technically: <code>PUT /api/kv/:key → 200</code>",
    notes: "<ol><li>You clicked <b>CREATE NOTE</b>.</li><li>The browser sent title and text to the server.</li><li>The server stored a numbered row in SQLite.</li></ol>Technically: <code>POST /api/notes → 201</code>",
    auth: "<ol><li>You tried a password.</li><li>The browser sent it as an <code>X-API-Key</code> header.</li><li>The server compared it in constant time and allowed or refused.</li></ol>Technically: <code>PUT … → 200 / 401 / 403</code>",
    echo: "<ol><li>You clicked <b>SEND</b>.</li><li>The browser sent your text as JSON.</li><li>The server echoed the same bytes back.</li></ol>Technically: <code>POST /api/echo → 200</code>",
    perf: "<ol><li>You clicked <b>RUN TEST</b>.</li><li>The browser fired N real requests at once.</li><li>It measured the wall time and counted errors.</li></ol>Timings include your local network."
  },
  pt: {
    save: "<ol><li>Você clicou em <b>GUARDAR</b>.</li><li>O navegador mandou nome e valor ao servidor C.</li><li>O servidor gravou no SQLite.</li><li>O navegador mostrou a resposta.</li></ol>Tecnicamente: <code>PUT /api/kv/:key → 200</code>",
    notes: "<ol><li>Você clicou em <b>CRIAR NOTA</b>.</li><li>O navegador mandou título e texto ao servidor.</li><li>O servidor guardou uma linha numerada no SQLite.</li></ol>Tecnicamente: <code>POST /api/notes → 201</code>",
    auth: "<ol><li>Você testou uma senha.</li><li>O navegador a enviou como header <code>X-API-Key</code>.</li><li>O servidor comparou em tempo constante e permitiu ou recusou.</li></ol>Tecnicamente: <code>PUT … → 200 / 401 / 403</code>",
    echo: "<ol><li>Você clicou em <b>ENVIAR</b>.</li><li>O navegador mandou seu texto como JSON.</li><li>O servidor devolveu os mesmos bytes.</li></ol>Tecnicamente: <code>POST /api/echo → 200</code>",
    perf: "<ol><li>Você clicou em <b>RODAR TESTE</b>.</li><li>O navegador disparou N requisições reais de uma vez.</li><li>Mediu o tempo e contou os erros.</li></ol>O tempo inclui sua rede local."
  },
  ru: {
    save: "<ol><li>Вы нажали <b>СОХРАНИТЬ</b>.</li><li>Браузер отправил имя и значение C-серверу.</li><li>Сервер записал их в SQLite.</li><li>Браузер показал ответ.</li></ol>Технически: <code>PUT /api/kv/:key → 200</code>",
    notes: "<ol><li>Вы нажали <b>СОЗДАТЬ ЗАМЕТКУ</b>.</li><li>Браузер отправил заголовок и текст.</li><li>Сервер сохранил строку с номером в SQLite.</li></ol>Технически: <code>POST /api/notes → 201</code>",
    auth: "<ol><li>Вы проверили пароль.</li><li>Браузер отправил его заголовком <code>X-API-Key</code>.</li><li>Сервер сравнил за константное время и разрешил или отказал.</li></ol>Технически: <code>PUT … → 200 / 401 / 403</code>",
    echo: "<ol><li>Вы нажали <b>ОТПРАВИТЬ</b>.</li><li>Браузер отправил текст как JSON.</li><li>Сервер вернул те же байты.</li></ol>Технически: <code>POST /api/echo → 200</code>",
    perf: "<ol><li>Вы нажали <b>ЗАПУСТИТЬ ТЕСТ</b>.</li><li>Браузер разом отправил N настоящих запросов.</li><li>Замерено время, посчитаны ошибки.</li></ol>Время включает вашу локальную сеть."
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
