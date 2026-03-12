import React, { useState, useEffect } from 'react';

function App() {
  const [gameState, setGameState] = useState(null);
  const [score, setScore] = useState(0);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    fetchGameState();
  }, []);

  const fetchGameState = async () => {
    try {
      const response = await fetch('/api/game/state');
      const data = await response.json();
      setGameState(data.state);
      setScore(data.score);
      setLoading(false);
    } catch (error) {
      console.error('Error fetching game state:', error);
      setLoading(false);
    }
  };

  const handleButtonClick = () => {
    alert('You clicked my button!');
  };

  const handleStartGame = async () => {
    try {
      const response = await fetch('/api/game/state', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ state: 'playing' }),
      });
      const data = await response.json();
      if (data.success) {
        setGameState('playing');
      }
    } catch (error) {
      console.error('Error starting game:', error);
    }
  };

  const handleResetGame = async () => {
    try {
      const response = await fetch('/api/game/reset', {
        method: 'POST',
      });
      const data = await response.json();
      if (data.success) {
        setGameState(data.state);
        setScore(data.score);
      }
    } catch (error) {
      console.error('Error resetting game:', error);
    }
  };

  if (loading) {
    return (
      <div className="container">
        <h1>Loading...</h1>
      </div>
    );
  }

  return (
    <div className="container">
      <h1>Game Interface</h1>
      <div className="game-info">
        <p><strong>State:</strong> {gameState || 'idle'}</p>
        <p><strong>Score:</strong> {score}</p>
      </div>
      <div className="button-group">
        <button onClick={handleButtonClick}>Click Me!</button>
        <button onClick={handleStartGame} className="btn-success">
          Start Game
        </button>
        <button onClick={handleResetGame} className="btn-warning">
          Reset Game
        </button>
      </div>
    </div>
  );
}

export default App;

