#!/bin/bash
# Script to run OCST Instance Visualizer

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Starting OCST Instance Visualizer...${NC}"

# Check if venv exists
if [ ! -d "venv" ]; then
    echo -e "${GREEN}Creating virtual environment...${NC}"
    python3 -m venv venv
    
    echo -e "${GREEN}Installing dependencies...${NC}"
    source venv/bin/activate
    pip install -r src/python/visualization/requirements.txt
else
    source venv/bin/activate
fi

echo -e "${GREEN}Launching Bokeh server...${NC}"
echo -e "${BLUE}Access the visualizer at: ${GREEN}http://localhost:5006/app${NC}"
echo -e "${BLUE}Press Ctrl+C to stop the server${NC}\n"

bokeh serve src/python/visualization/app.py --port 5006 --show

