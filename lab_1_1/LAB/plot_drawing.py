from turtle import *

WIDTH = 280
HEIGHT = 240


def main():
    file_bin = open("drawing_capture_1.txt", "r")
    drawing_bin = file_bin.read()
    print(len(drawing_bin))

    file_bin.close()

    screen = Screen()
    screen.setup(WIDTH*2, HEIGHT*2)
    screen.tracer(0)

    turtle = Turtle()
    turtle.color("black")
    turtle.hideturtle()
    turtle.speed(0)
    turtle.penup()

    point1 = (-WIDTH//2, HEIGHT//2)
    point2 = (WIDTH//2, HEIGHT//2)
    point3 = (WIDTH//2, -HEIGHT//2)
    point4 = (-WIDTH//2, -HEIGHT//2)


    # 2. Move to the first point
    turtle.goto(point1)
    turtle.pendown()
    turtle.goto(point2)
    turtle.goto(point3)
    turtle.goto(point4)
    turtle.goto(point1)
    turtle.penup()



    for j in range(0, HEIGHT):
        for i in range(0, WIDTH):
            turtle.goto(i - WIDTH//2, HEIGHT//2 - j)
            if drawing_bin[i + j * WIDTH] == "1":
                turtle.dot(1)

    screen.update()

    print("Drawing complete. Close the window to exit.")
    screen.mainloop()
            





if __name__ == "__main__":
    main()