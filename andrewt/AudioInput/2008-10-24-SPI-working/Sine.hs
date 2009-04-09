-- Generate a sine wave lookup table.
module Main ( main ) where

-- Number of samples.

samples :: Integer
samples = 256

-- Amplitude range (scaling factor).

range :: Double
range = fromInteger (2 ^ (16 - 3) - 1)

-- Build it backwards.
sineSamples :: [Double]
sineSamples = sample (2 * pi - delta)
  where
    sample :: Double -> [Double]
    sample x
        | x < 0      = []
        | otherwise  = sin x : sample (x - delta)

    delta :: Double
    delta = 2 * pi / (fromInteger samples)

scaleSamples :: [Double] -> [Int]
scaleSamples = map (\s -> round (s * range))

sineTable :: String
sineTable = snd (foldr f (0, id) (scaleSamples sineSamples)) ""
  where
    f :: Int -> (Int, ShowS) -> (Int, ShowS)
    f s (n, str)
      | n == 0          = (n + 1, str . ((++) ("\t" ++ show s)))
      | n `mod` 10 == 0 = (n + 1, str . ((++) ("\n\t, " ++ show s)))
      | otherwise       = (n + 1, str . ((++) (", " ++ show s)))

main :: IO ()
main = putStrLn $ "int16_t sine_table[" ++ show samples ++ "] = {\n" ++ sineTable ++ "\n};\n"
