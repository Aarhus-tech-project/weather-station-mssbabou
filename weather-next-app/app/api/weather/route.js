import { getLastWeatherData } from '@/app/lib/db';

export const runtime = 'nodejs';

export async function GET() {
    const data = await getLastWeatherData();
    return new Response(data, {
        headers: { 'Content-Type': 'application/json' }
    });
}