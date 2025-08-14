import { getLastWeatherData } from "@/app/lib/db";

export const runtime = 'nodejs';

export async function POST(req) {
  const { model = "qwen2.5:3b-instruct" } = (await safeJson(req)) ?? {};

  // --- local time → greeting (Europe/Copenhagen) ---
  const tz = "Europe/Copenhagen";
  const now = new Date();
  const hour = Number(
    new Intl.DateTimeFormat("en-GB", { hour: "numeric", hour12: false, timeZone: tz }).format(now)
  );
  const greeting =
    hour >= 5 && hour < 12 ? "Good morning" :
    hour >= 12 && hour < 18 ? "Good afternoon" :
    hour >= 18 && hour < 23 ? "Good evening" :
    "Hi";
  const nowISO = new Intl.DateTimeFormat("sv-SE", {
    timeZone: tz, year: "numeric", month: "2-digit", day: "2-digit",
    hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false
  }).format(now); // ISO-ish for readability

  const SYSTEM_RULES = [
    "You are an indoor weather reporter speaking casually, like chatting with a friend.",
    // hard requirement:
    `Begin your response with this exact greeting followed by a comma: "${greeting},"`,
    "Then write ONE short summary (1–2 sentences) about how the room feels overall.",
    "Do not list every sensor reading — only mention a number if it's unusually high/low and relevant.",
    "Keep it natural and human — emojis are fine.",
    "Skip anything that's normal or boring."
  ].join("\n");

  const d = await getLastWeatherData();

  const body = {
    model,
    stream: true,
    keep_alive: -1,
    messages: [
      { role: "system", content: SYSTEM_RULES },
      { role: "user", content: "Here is the latest indoor sensor snapshot as JSON." },
      { role: "user", content: JSON.stringify(d ?? {}) },
      {
        role: "user",
        content: JSON.stringify({
          local_time: nowISO,
          timezone: tz,
          hour_24: hour,
          greeting // the model must start with this
        })
      }
    ],
    options: {
      temperature: 0.3,
      num_predict: -1
    }
  };

  const upstream = await fetch("http://192.168.108.11:11434/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  });

  if (!upstream.ok || !upstream.body) {
    const msg = await upstream.text().catch(() => "Upstream error");
    return new Response(`Ollama error: ${msg}`, { status: 502 });
  }

  const decoder = new TextDecoder();
  const encoder = new TextEncoder();

  const stream = new ReadableStream({
    async start(controller) {
      const reader = upstream.body.getReader();
      let buf = '';
      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          buf += decoder.decode(value, { stream: true });

          let nl;
          while ((nl = buf.indexOf('\n')) !== -1) {
            const line = buf.slice(0, nl).trim();
            buf = buf.slice(nl + 1);
            if (!line) continue;

            try {
              const json = JSON.parse(line);
              const chunk = json?.message?.content;
              if (typeof chunk === "string" && chunk.length) {
                controller.enqueue(encoder.encode(chunk));
              }
              if (json?.done && json?.done_reason) {
                controller.enqueue(encoder.encode(`\n`));
              }
            } catch { /* ignore partials */ }
          }
        }
      } finally {
        controller.close();
      }
    }
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/plain; charset=utf-8",
      "Cache-Control": "no-cache, no-transform"
    }
  });
}

async function safeJson(req) {
  try { return await req.json(); } catch { return null; }
}