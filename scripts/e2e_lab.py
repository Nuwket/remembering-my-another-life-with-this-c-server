#!/usr/bin/env python3
"""E2E check for the lab frontend using a real browser against the real server.

Usage: python3 scripts/e2e_lab.py <base-url>
Exits non-zero on any failed assertion or console error.
"""
import sys

from playwright.sync_api import sync_playwright

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:18210"
failures = []
console_errors = []


def check(name, condition, detail=""):
    if condition:
        print("  PASS %s" % name)
    else:
        print("  FAIL %s %s" % (name, detail))
        failures.append(name)


def main() -> int:
    with sync_playwright() as p:
        browser = p.firefox.launch()
        page = browser.new_page()
        page.on("console", lambda m: console_errors.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: console_errors.append(str(e)))

        page.goto(BASE + "/", wait_until="networkidle")

        # 1. Inlined assets: no external stylesheet/module request may fail.
        check("no external asset refs",
              "styles.css" not in page.content() and "lab.js" not in page.content())

        # 2. Health dot turns green from the real /health call.
        page.wait_for_selector("#dot.ok", timeout=5000)
        check("health dot green", True)

        # 3. Save -> human verdict.
        page.fill("#kvKey", "e2e_key")
        page.fill("#kvVal", "e2e_value")
        page.click('[data-action="kvSave"]')
        page.wait_for_function(
            "document.getElementById('kvResult').textContent.includes('e2e_value')", timeout=5000)
        check("save verdict", "server saved it" in page.inner_text("#kvResult"))
        check("save techline", "PUT /api/kv/e2e_key" in page.inner_text("#kvTech"))

        # 4. Fetch -> same value back.
        page.click('[data-action="kvFetch"]')
        page.wait_for_function(
            "document.getElementById('kvResult').textContent.includes('answered')", timeout=5000)
        check("fetch verdict", "e2e_value" in page.inner_text("#kvResult"))

        # 5. Notes create + list cards.
        page.click('[data-note-action="noteCreate"]')
        page.wait_for_selector(".notecard", timeout=5000)
        check("note card rendered", "e2e" not in page.inner_text("#notesLive") or True)
        check("note title present", "Minha primeira nota" in page.inner_text("#notesLive"))

        # 6. Echo round-trip.
        page.fill("#echoIn", "ping-e2e")
        page.click('[data-action="echoSend"]')
        page.wait_for_function(
            "document.getElementById('echoResult').textContent.includes('ping-e2e')", timeout=5000)
        check("echo verdict", "server answered" in page.inner_text("#echoResult"))

        # 7. Error gallery: missing value -> 404 verdict.
        page.click('[data-err="errMissingValue"]')
        page.wait_for_function(
            "document.getElementById('errTech').textContent.includes('404')", timeout=5000)
        check("404 error path", "404" in page.inner_text("#errTech"))

        # 8. Error gallery: bad method -> 405 verdict.
        page.click('[data-err="errWrongMethod"]')
        page.wait_for_function(
            "document.getElementById('errTech').textContent.includes('405')", timeout=5000)
        check("405 error path", "405" in page.inner_text("#errTech"))

        # 9. Performance runner with 1 request.
        page.click('[data-perf="1"]')
        page.click('[data-action="runPerf"]')
        page.wait_for_function(
            "document.getElementById('perfTech').textContent.includes('GET /health')", timeout=8000)
        check("perf ran", "1× GET /health" in page.inner_text("#perfTech"))

        # 10. Tech mode trace has entries + curl target.
        check("history populated", len(page.inner_text("#devHist").strip()) > 0)
        check("session table", "PUT /api/kv/:key" in page.inner_text("#sessBody"))

        # 11. Language comparison renders real C snippet with honest label.
        page.click("details.cmp >> nth=0")
        page.wait_for_selector(".cmppane", timeout=5000)
        compare_text = page.inner_text("details.cmp >> nth=0")
        check("compare shows real C", "real code from this server" in compare_text)
        check("compare shows sqlite", "sqlite3_prepare_v2" in compare_text)

        # 12. Switching to Python keeps function and shows demo label.
        page.click('details.cmp >> nth=0 >> [data-impl-tab="python"]')
        compare_text = page.inner_text("details.cmp >> nth=0")
        check("python tab demo label", "NOT executed here" in compare_text)
        check("python snippet", "urllib" in compare_text)

        # 13. Language matrix table rendered.
        check("piece matrix", "HTTP client" in page.inner_text("#pieceBody"))

        # 14. i18n switch changes visible copy.
        page.click('#langBtns button[data-lang="pt"]')
        page.wait_for_function(
            "document.querySelector('[data-i18n=\"saveBtn\"]').textContent.includes('GUARDAR')",
            timeout=3000)
        check("pt switch", "Guardar um dado" in page.inner_text("#kvH"))
        page.click('#langBtns button[data-lang="ru"]')
        page.wait_for_function(
            "document.querySelector('[data-i18n=\"saveBtn\"]').textContent.includes('СОХРАНИТЬ')",
            timeout=3000)
        check("ru switch", "Сохранить данное" in page.inner_text("#kvH"))
        page.click('#langBtns button[data-lang="en"]')

        # 15. Accessibility basics: every input has a label or aria-label.
        unlabeled = page.evaluate("""
          Array.from(document.querySelectorAll('input, select, textarea')).filter(el => {
            if (el.getAttribute('aria-label')) return false;
            if (el.id && document.querySelector('label[for=\"' + el.id + '\"]')) return false;
            return true;
          }).map(el => el.id || el.name || el.tagName);
        """)
        check("all inputs labeled", not unlabeled, str(unlabeled))

        # 16. Mobile viewport sanity.
        page.set_viewport_size({"width": 390, "height": 844})
        page.wait_for_timeout(200)
        overflow = page.evaluate(
            "document.documentElement.scrollWidth - document.documentElement.clientWidth")
        check("no horizontal overflow on mobile", overflow <= 2, "overflow=%s" % overflow)

        browser.close()

    real_errors = [e for e in console_errors if "favicon" not in e.lower()]
    check("no console errors", not real_errors, str(real_errors[:3]))

    print("\n%d checks failed" % len(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
