const express = require('express');
const { createClient } = require('@supabase/supabase-js');
const app = express();
const PORT = 3000;

// Parse POST bodies
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Supabase setup
const SUPABASE_URL = "http://127.0.0.1:54321";
const SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZS1kZW1vIiwicm9sZSI6ImFub24iLCJleHAiOjE5ODM4MTI5OTZ9.CRXP1A7WOeoJeXxjNni43kdQwgnWNReilDMblYTn_I0";
const supabaseClient = createClient(SUPABASE_URL, SUPABASE_ANON_KEY);

// 1️⃣ Define dynamic route BEFORE static middleware
app.get('/news', async (req, res) => {
  try {
    const { data, error } = await supabaseClient
      .from('News')
      .select('*')
      .order('created_at', { ascending: false });

    if (error) throw error;

    res.json(data);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Erreur lors de la récupération des news' });
  }
});

// Serve static files
app.use(express.static('public'));
app.put('/news/:id', async (req, res) => {
  const { id } = req.params;
  const { title, content } = req.body;
  const { error } = await supabaseClient.from('News').update({ title, content }).eq('id', id);
  if (error) return res.status(500).json({ error: error.message });
  res.json({ message: 'News updated' });
});

// DELETE a news
app.delete('/news/:id', async (req, res) => {
  const { id } = req.params;
  const { error } = await supabaseClient.from('News').delete().eq('id', id);
  if (error) return res.status(500).json({ error: error.message });
  res.json({ message: 'News deleted' });
});
app.listen(PORT, () => console.log(`Server running on http://localhost:${PORT}`));
