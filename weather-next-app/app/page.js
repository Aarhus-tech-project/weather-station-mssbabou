'use client';

import { useRef, useState } from 'react';

export default function Home() {
  const [output, setOutput] = useState('');
  const [isStreaming, setIsStreaming] = useState(false);
  const abortRef = useRef(null);

  async function run() {
    setOutput('');
    setIsStreaming(true);
    const controller = new AbortController();
    abortRef.current = controller;

    try {
      const res = await fetch('/api/ollama', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        signal: controller.signal,
      });

      if (!res.ok || !res.body) {
        const msg = await res.text().catch(() => `HTTP ${res.status}`);
        throw new Error(msg || `HTTP ${res.status}`);
      }

      const reader = res.body.getReader();
      const decoder = new TextDecoder();

      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        const chunk = decoder.decode(value, { stream: true });
        setOutput((prev) => prev + chunk);
      }
    } catch (err) {
      if (err.name !== 'AbortError') {
        setOutput((p) => p + `\n\n[Error] ${String(err)}`);
      }
    } finally {
      setIsStreaming(false);
      abortRef.current = null;
    }
  }

  function stop() {
    abortRef.current?.abort();
  }

  return (
    <main style={{ maxWidth: 720, margin: '0 auto', padding: 16 }}>
      <h1>Ollama Streaming Proxy (JS)</h1>
      <div style={{ display: 'flex', gap: 8, marginTop: 8 }}>
        <button onClick={run} disabled={isStreaming}>
          {isStreaming ? 'Streaming…' : 'Generate'}
        </button>
        <button onClick={stop} disabled={!isStreaming}>
          Stop
        </button>
      </div>

      <pre
        style={{
          whiteSpace: 'pre-wrap',
          minHeight: 120,
          padding: 12,
          border: '1px solid #ddd',
          marginTop: 12,
          borderRadius: 8,
        }}
      >
        {output || '— output will stream here —'}
      </pre>
    </main>
  );
}
