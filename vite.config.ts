import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// Relative base so the build works on GitHub Pages project URLs and locally.
export default defineConfig({
  base: './',
  plugins: [react({ exclude: [/src\/firmware/] })],
})
