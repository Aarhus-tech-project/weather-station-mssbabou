// app/api/ollama/route.js
export const runtime = 'nodejs';

export async function POST(req) {
    const prompt = 'clanker'; 
    const model =  'gpt-oss:latest'; //'qwen2.5:3b-instruct';

  const upstream = await fetch('http://localhost:11434/api/generate', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    // Ollama streams JSONL when stream=true
    body: JSON.stringify({ model, prompt, stream: true }),
  });

  if (!upstream.ok || !upstream.body) {
    const msg = await upstream.text().catch(() => 'Upstream error');
    return new Response(`Ollama error: ${msg}`, { status: 502 });
    // (If it's a network error, ^ will be generic.)
  }

  const decoder = new TextDecoder();
  const encoder = new TextEncoder();

  // Convert Ollama JSONL → plain text chunks (token by token)
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
              if (json && typeof json.response === 'string' && json.response.length) {
                controller.enqueue(encoder.encode(json.response));
              }
              if (json && json.done) break;
            } catch {
              // ignore partial JSON
            }
          }
        }
      } catch (e) {
        controller.error(e);
        return;
      } finally {
        controller.close();
      }
    },
  });

  return new Response(stream, {
    headers: {
      'Content-Type': 'text/plain; charset=utf-8',
      'Cache-Control': 'no-cache, no-transform',
    },
  });
}
